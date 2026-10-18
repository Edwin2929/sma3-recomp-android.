"""Optional castle health observations; these checks do not certify route progress."""
import struct

CASTLE_STATUS_PROFILE = 'castle-status-v1'
IWRAM_BASE, IWRAM_SIZE = 0x03000000, 0x8000
GAME_POINTER = 0x0300220C
STATUS_RANGES = {
    'game_pointer': (0, 0xffffffff),
    'lives': (-32768, 32767),
    'deaths': (0, 65535),
    'inhibition': (0, 255),
    'stars': (0, 65535),
    'entry_type': (0, 65535),
    'exit_index': (0, 65535),
    'baby_id': (0, 65535),
    'baby_status': (0, 65535),
}


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _validate_pointer(pointer, game_state):
    _require(type(pointer) is int and IWRAM_BASE <= pointer < IWRAM_BASE + IWRAM_SIZE and
             pointer % 4 == 0 and pointer + 0x2AAE <= IWRAM_BASE + IWRAM_SIZE,
             'Invalid castle game pointer or IWRAM extent')
    _require(game_state != 13 or pointer == GAME_POINTER,
             'Unexpected castle game pointer during gameplay')


def validate_castle_status(status, game_state):
    """Validate JSON observations without imposing life, death or progress gates."""
    _require(type(status) is dict and set(status) == set(STATUS_RANGES),
             'Invalid castle_status fields')
    _require(type(game_state) is int and 0 <= game_state <= 255, 'Invalid castle game state')
    for name, (minimum, maximum) in STATUS_RANGES.items():
        _require(type(status[name]) is int and minimum <= status[name] <= maximum,
                 f'Invalid castle_status integer or range: {name}')
    _validate_pointer(status['game_pointer'], game_state)


def parse_castle_status(iwram):
    """Read one complete IWRAM snapshot; validate P before any relative access."""
    _require(type(iwram) in (bytes, bytearray) and len(iwram) == IWRAM_SIZE,
             'Castle observations require exactly 32768 IWRAM bytes')
    pointer = struct.unpack_from('<I', iwram, 0x7240)[0]
    _validate_pointer(pointer, iwram[0x6b05])
    base = pointer - IWRAM_BASE
    return dict(game_pointer=pointer,
                lives=struct.unpack_from('<h', iwram, 0x627E)[0],
                deaths=struct.unpack_from('<H', iwram, 0x634C)[0],
                inhibition=iwram[0x6351],
                stars=struct.unpack_from('<H', iwram, 0x6ACE)[0],
                entry_type=struct.unpack_from('<H', iwram, 0x6A52)[0],
                exit_index=struct.unpack_from('<H', iwram, base + 0x3E)[0],
                baby_id=struct.unpack_from('<H', iwram, base + 0x254 + 0x32)[0],
                baby_status=struct.unpack_from('<H', iwram, base + 0x254 + 0x24)[0])


def validate_castle_history(session, trace):
    """If enabled anywhere, require valid health data everywhere and bind endpoints."""
    rows = [*trace, *session]
    if not any('castle_status' in row for row in rows):
        return False
    _require(all('castle_status' in row for row in rows), 'Partial castle_status history')
    for row in rows:
        validate_castle_status(row['castle_status'], row['game_state'])
    for end in session:
        elapsed = end['elapsed_frame']
        _require(type(elapsed) is int and 0 <= elapsed < len(trace), 'Invalid castle checkpoint frame')
        _require(end['castle_status'] == trace[elapsed]['castle_status'],
                 'Castle checkpoint differs from frame trace')
    return True
