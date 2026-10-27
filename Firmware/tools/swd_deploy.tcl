# Toolkit-owned result contract over OpenOCD APIs. No native log grammar assumed.
# Run only after reviewed SAFE hardware prerequisites: init/reset init halts/reset.
proc wtk_flash_bytes {} {
    set bytes 65536
    if {[info exists ::env(WTK_FLASH_BYTES)]} {set bytes $::env(WTK_FLASH_BYTES)}
    if {$bytes ne "65536" && $bytes ne "131072"} {error "unsupported configured Flash bytes"}
    if {$bytes eq "131072" && $::env(WTK_UID) ne "54ff70068370525760380567"} {
        error "128 KiB is verified only on the explicitly selected first unit"
    }
    return $bytes
}
proc wtk_preflight {} {
    global _TARGETNAME
    if {[$_TARGETNAME curstate] ne "halted"} { error "target not halted" }
    flash probe 0
    set banks [flash list]
    if {[llength $banks] != 1} { error "ambiguous flash banks" }
    set bank [lindex $banks 0]
    set device [lindex [read_memory 0xe0042000 32 1] 0]
    set cpu [lindex [read_memory 0xe000ed00 32 1] 0]
    set kb [lindex [read_memory 0x1ffff7e0 16 1] 0]
    set obr [lindex [read_memory 0x4002201c 32 1] 0]
    set wrpr [lindex [read_memory 0x40022020 32 1] 0]
    set uid ""
    foreach word [read_memory 0x1ffff7e8 32 3] {
        for {set byte 0} {$byte < 4} {incr byte} {
            append uid [format "%02x" [expr {($word >> ($byte*8)) & 255}]]
        }
    }
    if {($device & 4095) != 0x410 || ($cpu >> 24) != 0x41 || (($cpu >> 4) & 4095) != 0xc23} { error "unsupported MCU/core" }
    set capacity [wtk_flash_bytes]
    if {$capacity == 131072 && $uid ne "54ff70068370525760380567"} {error "unverified extended-Flash UID"}
    if {$kb != 64 || [dict get $bank base] != 0x08000000 || [dict get $bank size] != $capacity || [dict get $bank driver] ne "stm32f1x"} { error "unsupported flash geometry" }
    if {($obr & 3) != 0 || $wrpr != 0xffffffff} { error "readout/option/write protection: STOP, no unlock" }
    if {$::env(WTK_UID) ne "" && $uid ne $::env(WTK_UID)} { error "target UID mismatch" }
    echo [format "WTK_B011_TARGET|%08x|%08x|%d|%08x|%08x|%s|%d|%d|%s|halted" $device $cpu $kb $obr $wrpr $uid [dict get $bank base] [dict get $bank size] [dict get $bank driver]]
}

proc wtk_operation {} {
    global _TARGETNAME
    wtk_preflight
    set capacity [wtk_flash_bytes]
    if {$::env(WTK_ACTION) eq "program"} {
        set length $::env(WTK_LENGTH)
        if {$length < 8 || $length > $capacity || ($length % 2) != 0} { error "invalid image length" }
        dump_image $::env(WTK_BEFORE) 0x08000000 $capacity
        # Never request the whole bank in one erase operation: the stm32f1x driver
        # otherwise silently optimizes first=0,last=63 to MASS ERASE.
        set pages [expr {($length + 1023) / 1024}]
        for {set page 0} {$page < $pages} {incr page} {
            if {[$_TARGETNAME curstate] ne "halted"} { error "reset/run during erase" }
            flash erase_sector 0 $page $page
        }
        flash write_bank 0 $::env(WTK_IMAGE) 0
        if {[$_TARGETNAME curstate] ne "halted"} { error "reset/run after programming" }
        echo "WTK_B011_PROGRAMMED"
    }
    if {$::env(WTK_ACTION) ne "probe"} {
        dump_image $::env(WTK_READBACK) 0x08000000 $capacity
        if {[$_TARGETNAME curstate] ne "halted"} { error "reset/run during readback" }
        echo "WTK_B011_READBACK|134217728|$capacity|halted"
    }
    echo "WTK_B011_COMPLETE"
}

# The launcher supplies only fixed commands and environment DATA, never Tcl-evaluated paths.
