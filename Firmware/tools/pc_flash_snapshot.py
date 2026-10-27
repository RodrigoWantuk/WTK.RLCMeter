#!/usr/bin/env python3
"""Read-only PRODUCT W25Q preservation snapshot; never resets, erases or programs."""
from __future__ import annotations
import argparse,hashlib,json,struct,time,zlib
from pathlib import Path
from pc_serial_ui import HEADER,MAGIC,exchange,TELEMETRY
from pc_bringup import open_serial,utc_now
from storage_layout import SUPPORTED_CAPACITIES,resource_limit
COMMAND,RESPONSE,REPLY_BYTES,CHUNK=0x33,0xB3,128,108

def encode(identity,session,address,count):
    if not 0<identity<=0xffffffff or not 0<=session<=0xffffffff or not 0<=address<=0xffffffff or not 1<=count<=CHUNK:
        raise ValueError('invalid read request')
    payload=struct.pack('<BBHIIIHH',1,COMMAND,0,identity,session,address,count,0)
    return HEADER.pack(MAGIC,1,COMMAND,0,identity&65535,len(payload),zlib.crc32(payload))+payload

def decode(raw,identity,session,address,count):
    if len(raw)!=HEADER.size+REPLY_BYTES:raise ValueError('incomplete flash reply')
    payload=raw[HEADER.size:]
    if HEADER.unpack_from(raw)!=(MAGIC,1,RESPONSE,0,identity&65535,REPLY_BYTES,zlib.crc32(payload)):
        raise ValueError('flash reply framing/CRC mismatch')
    api,echo,status,_state,received_id,received_session,received_address,n,error,reserved=struct.unpack_from('<BBBBIIIHBB',payload)
    if (api,echo,status,received_id,received_session,received_address,n,error,reserved)!=(1,COMMAND,0,identity,session,address,count,0,0):
        raise ValueError('flash read rejected or identity/address mismatch')
    return payload[20:20+count]

def read_chunk(link,identity,session,address,count,tx,rx):
    sent=encode(identity,session,address,count);tx.write(sent)
    if link.write(sent)!=len(sent):raise ValueError('short read-command transmission; no retry')
    link.flush();received=bytearray();deadline=time.monotonic()+3.0
    while len(received)<HEADER.size+REPLY_BYTES and time.monotonic()<deadline:
        received.extend(link.read(HEADER.size+REPLY_BYTES-len(received)))
    rx.write(received)
    return decode(bytes(received),identity,session,address,count)

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4');parser.add_argument('--capacity',type=int,choices=SUPPORTED_CAPACITIES,required=True)
    parser.add_argument('--expected-uid',required=True);parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compare',type=Path,help='require exact equality with a previous tail.bin')
    args=parser.parse_args(argv);limit=resource_limit(args.capacity)
    if len(args.expected_uid)!=24 or any(c not in '0123456789abcdef' for c in args.expected_uid):parser.error('need lowercase 12-byte UID')
    previous=args.compare.read_bytes() if args.compare else None
    if previous is not None and len(previous)!=40960:parser.error('comparison must be an exact 40-KiB snapshot')
    args.out.mkdir(parents=True,exist_ok=False)
    record=dict(utc=utc_now(),operation='READ_ONLY_W25Q_SNAPSHOT',port=args.port,capacity=args.capacity,
        reset_requested=False,activation_requested=False,flash_written=False,automatic_retry=False)
    identity=int(time.time()*1000)&0xffffffff
    if not 0<identity<0xffffffff-1000:raise ValueError('request-ID boundary; stop without sending')
    link=open_serial(args.port)
    try:
        link.reset_input_buffer();time.sleep(0.3)
        status,_,_=exchange(link,TELEMETRY,identity,0,args.out,'telemetry')
        record['telemetry']=status
        if status['status']!='ACCEPTED' or status['uid']!=args.expected_uid or status['k1_commanded_state']!=1 or status['range_state']!=0:
            raise ValueError('unit/SAFE identity required')
        session=status['session_sequence'];identity+=1
        with (args.out/'read-tx.bin').open('wb') as tx,(args.out/'read-rx.bin').open('wb') as rx:
            header=read_chunk(link,identity,session,0,44,tx,rx);identity+=1
            (args.out/'resource-header.bin').write_bytes(header)
            if struct.unpack_from('<I',header)[0]!=0x32505257:raise ValueError('resource header not recognized; no writes')
            pack_end=struct.unpack_from('<I',header,12)[0]
            record['resource_pack_end']=pack_end;record['resource_limit']=limit
            tail=bytearray()
            while len(tail)<40960:
                count=min(CHUNK,40960-len(tail))
                tail.extend(read_chunk(link,identity,session,limit+len(tail),count,tx,rx));identity+=1
            (args.out/'tail.bin').write_bytes(tail)
        record['sha256']=hashlib.sha256(tail).hexdigest()
        record['legacy_tail_sha256']=hashlib.sha256(tail[8192:]).hexdigest()
        record['lead_sectors_sha256']=hashlib.sha256(tail[:8192]).hexdigest()
        record['resource_header_sha256']=hashlib.sha256(header).hexdigest()
        record['comparison_equal']=previous==tail if previous is not None else None
        if pack_end>limit:raise ValueError('resource overlaps new lead sectors; repack required before compensation')
        if previous is not None and previous!=tail:raise ValueError('preserved bytes differ; inspect before further work')
        record['outcome']='SNAPSHOT_VERIFIED'
    except Exception as exc:
        record['outcome']='STOPPED_NO_RETRY';record['error']=str(exc);raise
    finally:
        link.close();(args.out/'report.json').write_text(json.dumps(record,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(json.dumps(record,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
