#!/usr/bin/env python3
"""Real PLC1 OSL installer. Success requires device verification AND exact readback."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct
import time
import zlib

import pc_osl
import inspect_calibration_record as wire
from pc_cal_capture import Client, FakeSerial, CaptureError, STANDARDS, save_capture

BEGIN, CHUNK, VALIDATE, COMMIT, STATUS, READBACK, ABORT = range(0x55,0x5c)
IDLE, RECEIVING, VALIDATED, WRITING, INSTALLED, FAILED, ABORTED = range(7)


def validate_candidate(frame):
    summary=pc_osl.validate_frame(frame)
    if len(frame)!=2760 or struct.unpack_from('<H',frame,22)[0]!=1 or frame[24:56]!=bytes(32) or (
            struct.unpack_from('<HI',frame,66)!=(0,1)):
        raise CaptureError('noncanonical candidate metadata')
    adc=struct.unpack_from('<12f',frame,72)
    if not all(math.isfinite(x) for x in adc) or any(x<=0 for x in adc[::2]):
        raise CaptureError('invalid ADC fields')
    for i in range(33):
        offset=120+i*80
        flags=struct.unpack_from('<I',frame,offset+18)[0]
        if flags&~0x2e0 or flags&0x220!=0x220:
            raise CaptureError('candidate cannot assert QUALIFIED or unknown flags')
        values=struct.unpack_from('<12f',frame,offset+22)
        hg,load,short,opened,k,reserved=[complex(*values[j:j+2]) for j in range(0,12,2)]
        temperature=struct.unpack_from('<i',frame,offset+10)[0]
        if (not all(math.isfinite(x) for x in values) or abs(hg)<=1e-6 or abs(load)<=1e-6 or
            load.real<0 or abs(short-opened)<=1e-5 or abs(k)<=1e-6 or reserved!=0 or
            not -40000<=temperature<=125000 or (not flags&64 and temperature!=0) or
            frame[offset+70:offset+80]!=bytes(10)):
            raise CaptureError('invalid numerical coefficients/metadata')
    return summary


class Installer:
    def __init__(self, client):
        self.client=client

    def status(self):
        _,data=self.client.command(STATUS)
        if len(data)!=32:raise CaptureError('invalid installation status')
        transaction,sequence,received,size,active_sequence,crc=struct.unpack_from('<IIHHII',data)
        return dict(transaction=transaction,sequence=sequence,received=received,size=size,
                    active_sequence=active_sequence,crc=crc,state=data[20],error=data[21],store_state=data[22],
                    storage_available=bool(data[23]),next_sequence=struct.unpack_from('<I',data,24)[0],
                    slot=data[28],active_valid=bool(data[29]))

    def begin(self, frame):
        summary=validate_candidate(frame)
        if self.client.identity is None:self.client.identify()
        if not self.client.identity['capabilities']&16:raise CaptureError('installation not supported')
        state=self.status()
        if not state['storage_available']:raise CaptureError('storage unavailable')
        if summary.sequence!=state['next_sequence'] or not state['next_sequence']:
            raise CaptureError('candidate sequence differs from device successor; rollover forbidden')
        return self.client.command(BEGIN,struct.pack('<III',len(frame),zlib.crc32(frame),summary.sequence))[0]

    def transfer(self, transaction, frame):
        for offset in range(0,len(frame),96):
            self.client.command(CHUNK,struct.pack('<IH',transaction,offset)+frame[offset:offset+96])

    def validate(self,transaction):self.client.command(VALIDATE,struct.pack('<I',transaction))
    def commit(self,transaction):self.client.command(COMMIT,struct.pack('<I',transaction))
    def abort(self,transaction):self.client.command(ABORT,struct.pack('<I',transaction))

    def readback(self,sequence):
        frame=bytearray();expected_crc=None
        for offset in range(0,2760,96):
            count=min(96,2760-offset)
            _,part=self.client.command(READBACK,struct.pack('<IHBB',sequence,offset,count,0))
            if len(part)!=12+count:raise CaptureError('readback length mismatch')
            seq,position,size,crc=struct.unpack_from('<IHHI',part)
            if (seq,position,size)!=(sequence,offset,2760) or (expected_crc is not None and crc!=expected_crc):
                raise CaptureError('readback identity mismatch')
            expected_crc=crc;frame.extend(part[12:])
        summary=pc_osl.validate_frame(bytes(frame))
        if summary.sequence!=sequence or struct.unpack_from('<I',frame,56)[0]!=expected_crc:
            raise CaptureError('readback sequence/CRC mismatch')
        return bytes(frame)

    def install(self,frame,report_path=None,readback_path=None):
        transaction=self.begin(frame)
        started=datetime.now(timezone.utc).isoformat()
        try:
            self.transfer(transaction,frame);self.validate(transaction);self.commit(transaction)
            deadline=time.monotonic()+35
            while True:
                state=self.status()
                if state['transaction']!=transaction:raise CaptureError('installation identity mismatch')
                if state['state']!=WRITING:break
                if time.monotonic()>=deadline:raise TimeoutError('installation timeout')
                if not isinstance(self.client.transport,FakeSerial):time.sleep(0.01)
            if state['state']!=INSTALLED or state['error']:
                raise CaptureError('device installation failed: '+str(state))
            persisted=self.readback(state['active_sequence'])
            if persisted!=frame:raise CaptureError('persisted frame differs from candidate')
            report=dict(device=self.client.identity,transaction=transaction,started_utc=started,
                        completed_utc=datetime.now(timezone.utc).isoformat(),status=state,
                        candidate_sha256=hashlib.sha256(frame).hexdigest(),
                        readback_sha256=hashlib.sha256(persisted).hexdigest(),
                        verified=True,physically_qualified=False,evidence='REQUIRES_BENCH_VALIDATION')
            if readback_path:Path(readback_path).write_bytes(persisted)
            if report_path:Path(report_path).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            return report
        except (ValueError,TimeoutError,OSError):
            try:self.abort(transaction)
            except (ValueError,TimeoutError,OSError):pass
            # An ambiguous commit must be resolved by status/readback, never blind replay.
            raise


def provision_simulation(bridge,output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    transport=FakeSerial(bridge)
    try:
        client=Client(transport,first_id=1);client.identify()
        if client.status()['calibrated']:raise CaptureError('fixture is not blank')
        campaign=output/'campaign.json'
        if campaign.exists():raise CaptureError('use an empty output directory for the blank workflow')
        for key in pc_osl.KEYS:
            for standard in STANDARDS:
                reference=dict(id=f'SYNTHETIC_{key.rref_ohms}',impedance_ohms=[key.rref_ohms,0],tolerance_pct=1.0)
                save_capture(campaign,client.capture(key,standard,reference))
        installer=Installer(client)
        document=json.loads(campaign.read_text())
        frame,_=pc_osl.build_candidate(document,installer.status()['next_sequence'])
        (output/'candidate.bin').write_bytes(frame)
        report=installer.install(frame,output/'installation.json',output/'readback.bin')
        transport.control(4,struct.pack('<H',0))  # Test-only reset; no real serial reset command.
        client=Client(transport,first_id=1);client.identify()
        state=client.status()
        if not state['calibrated'] or state['calibration_sequence']!=1:raise CaptureError('reboot recovery failed')
        if Installer(client).readback(1)!=frame:raise CaptureError('reboot readback mismatch')
        measurement=transport.control(8,b'')
        status,sequence,corrected,source,re,im=struct.unpack('<4I2f',measurement)
        # Synthetic quantized samples and channel timing are not an accuracy qualification.
        if status or sequence!=1 or not corrected or source!=2 or abs(complex(re,im)-600)>12:
            raise CaptureError('persisted C runtime standalone measurement failed')
        report.update(captures=99,conditions=33,reboot_verified=True,
                      synthetic_standalone_z_ohms=[re,im],evidence='REQUIRES_BENCH_VALIDATION')
        boot=struct.unpack('<8I2f',transport.control(9,bytes((0,0,0))))
        if boot[0]!=4 or boot[1]!=1 or boot[2]!=1 or boot[3]!=0:
            raise CaptureError('actual PRODUCT boot gate failed or auto-started acquisition')
        measured=struct.unpack('<8I2f',transport.control(9,bytes((0,0,1))))
        if measured[0]!=20 or not measured[3] or not measured[4] or not measured[5] or measured[6]!=2:
            raise CaptureError('actual PRODUCT measurement session failed')
        report.update(product_boot_verified=True,factory_product=bool(boot[7]),
                      product_attempts=measured[3],product_z_ohms=list(measured[8:10]))
        (output/'installation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        return report
    finally:transport.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('status','install','readback','simulate'))
    parser.add_argument('--port');parser.add_argument('--candidate',type=Path)
    parser.add_argument('--report',type=Path,default=Path('installation-report.json'))
    parser.add_argument('--readback',type=Path,default=Path('installed-calibration.bin'))
    parser.add_argument('--bridge',type=Path);parser.add_argument('--output',type=Path,default=Path('provisioning-demo'))
    args=parser.parse_args()
    if args.action=='simulate':
        if not args.bridge:parser.error('--bridge required')
        print(json.dumps(provision_simulation(args.bridge,args.output),indent=2));return
    if not args.port:parser.error('--port required')
    frame=args.candidate.read_bytes() if args.action=='install' and args.candidate else None
    if args.action=='install':
        if frame is None:parser.error('--candidate required')
        validate_candidate(frame)  # Reject locally before connecting or writing.
    import serial
    with serial.Serial(args.port,115200,timeout=0.05,write_timeout=1) as transport:
        client=Client(transport);print(json.dumps(client.identify()))
        installer=Installer(client);state=installer.status()
        if args.action=='status':print(json.dumps(state,indent=2))
        elif args.action=='install':print(json.dumps(installer.install(frame,args.report,args.readback),indent=2))
        else:args.readback.write_bytes(installer.readback(state['active_sequence']))


if __name__=='__main__':main()
