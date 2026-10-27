"""W25Q resource ceiling; preserve all legacy mutable offsets at capacity - 32 KiB."""
LEGACY_MUTABLE_BYTES = 32768
LEAD_RESERVED_BYTES = 8192
SUPPORTED_CAPACITIES = (2*1024*1024, 4*1024*1024, 8*1024*1024, 16*1024*1024)

def resource_limit(capacity: int) -> int:
    if capacity not in SUPPORTED_CAPACITIES:
        raise ValueError("unsupported W25Q capacity")
    return capacity - LEGACY_MUTABLE_BYTES - LEAD_RESERVED_BYTES
