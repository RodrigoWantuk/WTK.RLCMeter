# SYNTHETIC API values. Executes the production Tcl with OpenOCD's real Jim interpreter,
# but never init/source an adapter or target configuration. Not a native SWD log fixture.
set _TARGETNAME wtk_mock_target
proc wtk_mock_target {action} { return halted }
proc read_memory {address width count} {
    switch $address {
        0xe0042000 { return {0x20036410} }
        0xe000ed00 { return {0x411fc231} }
        0x1ffff7e0 { return {64} }
        0x4002201c { return {0} }
        0x40022020 { return {0xffffffff} }
        0x1ffff7e8 { return {0x04030201 0x08070605 0x0c0b0a09} }
        default { error "unexpected memory access" }
    }
}
set wtk_pages {}
proc flash {action args} {
    global wtk_pages
    switch $action {
        probe { return 0 }
        list { return {{name stm32f1x.flash driver stm32f1x base 134217728 size 65536 target stm32f1x.cpu}} }
        erase_sector {
            if {[lindex $args 0] != 0 || [lindex $args 1] != [lindex $args 2]} {error "bulk/wrong-bank erase rejected"}
            lappend wtk_pages [lindex $args 1]
        }
        write_bank {
            if {[lindex $args 0] != 0 || [lindex $args 2] != 0} {error "wrong write range"}
            echo "WTK_TEST_PATH|[lindex $args 1]"
        }
        default { error "unexpected/destructive command" }
    }
}
proc dump_image {filename address size} {
    if {$address != 134217728 || $size != 65536} {error "wrong memory dump"}
    echo "WTK_TEST_DUMP|$filename"
}
source $::env(WTK_LIBRARY)
wtk_operation
echo "WTK_TEST_PAGES|[llength $wtk_pages]"
echo "SIMULATED API mocks: no target/adapter init executed"
shutdown
