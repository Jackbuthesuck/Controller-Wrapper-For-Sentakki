# Multi-Monitor Touch Input Testing Checklist

## Current Status
✅ **Almost Complete**: All monitor arrangements tested and working except for different resolutions

### Verified Working Configurations:
- ✅ Primary LEFT, Secondary RIGHT (horizontal)
- ✅ Primary RIGHT, Secondary LEFT (horizontal)
- ✅ Primary ABOVE Secondary (vertical)
- ✅ Primary BELOW Secondary (vertical)
- ✅ Primary Top-Right of Secondary (diagonal)
- ✅ Primary Bottom-Left of Secondary (diagonal)
- ✅ Edge positions and corner touches
- ✅ Monitor switching
- ⏳ **Pending**: Different resolutions between monitors

## Test Cases to Verify

### Basic Arrangements
- [x] **Primary LEFT, Secondary RIGHT**
  - Display 2 (primary) on LEFT, Display 1 (secondary) on RIGHT
  - Test touches on both Display 1 and Display 2
  - Verify touches go to the correct monitor

- [x] **Primary RIGHT, Secondary LEFT**
  - Display 2 (primary) on RIGHT, Display 1 (secondary) on LEFT
  - Touches working correctly on both monitors

### Vertical Arrangements
- [x] **Primary ABOVE Secondary**
  - Display 2 (primary) above Display 1 (secondary)
  - Primary monitor: Fixed to use opposite workaround for Y coordinates
  - Secondary monitor: Uses virtual coordinates (working correctly)

- [x] **Primary BELOW Secondary**
  - Display 2 (primary) below Display 1 (secondary)
  - Verify Y coordinate mapping works correctly
  - Test touches on both monitors

### Diagonal/Offset Arrangements
- [x] **Primary Top-Right of Secondary**
  - Display 2 (primary) positioned diagonally (above and to the right)
  - Both X and Y coordinates map correctly using diagonal formula
  - Formula: `touchX = (secondaryMonitorInfo->width / 2) + horizontalOffset`

- [x] **Primary Bottom-Left of Secondary**
  - Display 2 (primary) positioned diagonally (below and to the left)
  - Both X and Y coordinates map correctly
  - Tested with various horizontal/vertical offsets

### Edge Cases
- [x] **Edge Positions**
  - Tested touches at corners (0,0), (width,0), (0,height), (width,height)
  - Tested touches at edges (middle of left/right/top/bottom edges)
  - Coordinates are within bounds and working correctly

- [ ] **Different Resolutions**
  - If monitors have different resolutions, verify coordinate scaling works
  - Test touches at various positions to ensure proper scaling
  - **TODO: Test when monitors have different resolutions**

### Dynamic Behavior
- [x] **Monitor Switching**
  - Move cursor rapidly between monitors
  - Verify overlay switches smoothly
  - Verify touches route to the correct monitor immediately after switching

## Implementation Notes

The current implementation:
- **Horizontal arrangements (LEFT/RIGHT)**:
  - Primary LEFT: Uses `virtualX` directly
  - Primary RIGHT (on primary): Uses `relativeX + currentInfo->width` for purely horizontal
  - Primary RIGHT (on secondary): Uses `mappedX` (opposite monitor workaround)
  
- **Vertical arrangements (ABOVE/BELOW)**:
  - Primary ABOVE (on primary): Uses `virtualY` directly
  - Primary BELOW (on primary, purely vertical): Uses `relativeY + currentInfo->height`
  - Primary BELOW (on secondary): Uses `mappedY`
  
- **Diagonal arrangements (RIGHT cases)**:
  - Uses formula: `touchX = (secondaryMonitorInfo->width / 2) + horizontalOffset`
  - This ensures correct X coordinate mapping for diagonal RIGHT cases (both ABOVE and BELOW)
  - Y coordinate uses appropriate vertical workaround based on arrangement

- **General coordinate transformation**:
  - Detects monitor arrangement (primaryIsLeft, primaryIsRight, primaryIsAbove, primaryIsBelow)
  - Calculates horizontal offset for diagonal detection
  - Applies appropriate coordinate transformation based on current monitor and arrangement

## Debug Output

Check console output for:
- Monitor positions and sizes
- Primary/secondary status
- Coordinate calculations (relative, mappedRelative, mappedVirtual, final)
- Arrangement detection (otherIsLeft, otherIsRight, etc.)