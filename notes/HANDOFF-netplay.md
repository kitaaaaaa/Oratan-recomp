# Handoff: test the netplay prototype

For the Claude instance on the owner's Windows PC. Written by the cloud session, which has no
game or build. Delete this file when done, and add a "Netplay" section to `notes/DEV_DIARY.md`
with the results.

## What changed

**Lockstep netplay, built as a live replay.**
- Each machine sends its own controller's 60 bytes every tick, scheduled `netplay_delay` ticks
  ahead.
- Both machines write the same controller buffer: host in slot 0, joiner in slot 1, slots 2–3
  zero. If the other machine's input for a tick hasn't arrived, the game waits for it.
- Both exchange their per-tick state fingerprints. The first differing tick is logged with the
  regions that differ, and the state at that tick is written to
  `netplay_tick<N>_<host|join>.bin` in the working folder.
- Netplay forces local versus on and split screen off. Each machine draws its own robot's
  view during fights; the camera is changed only around Draw, like split screen, so the game
  state stays identical.
- The design and the packet format are described at the top of `src/netplay.cpp`.

**Files:**
- `src/netplay.cpp` / `.h`: the session, the UDP code (Winsock), and the packets.
- `src/tick_hooks.cpp` / `.h`: the Update and controller-read hooks moved out of `replay.cpp`,
  calling netplay and then replay. A replay recording made during netplay stores the merged
  inputs.
- `src/state_hash.cpp` / `.h`: the fingerprint moved out of `replay.cpp`, plus a capture
  function for exact-tick dumps.
- `src/replay.cpp`: uses those two. The recording format is unchanged (version 2).
- `src/local_versus.cpp`: `LocalVersusOn()` = the local versus setting or netplay active; no
  split screen in netplay; the netplay camera.
- `CMakeLists.txt`: the new files, and `ws2_32` on Windows.

**Tested in the cloud:** two separate processes running the real netplay, tick-hook, replay
and fingerprint code against a toy game over UDP on localhost.
- With different physical input on each side, both end identical.
- With 40 ms each way plus 20% packet loss, they stay in sync. About one tick in seven waited
  ~20 ms at a 3-tick delay; with no loss, 4–8 short stalls in 30 s.
- A forced divergence is caught at exactly its tick on both sides, with dumps.
- Mismatched game files are refused on both sides.
- When the joiner vanished, the host timed out after `netplay_timeout` and continued offline.
- The refactored replay still records and replays identically and catches drift at the exact
  tick.
- Syntax-checked for Windows against the SDK headers; not built with the real SDK.

## Settings (command line for now; no F1 menu yet)

| Setting | Meaning |
|---|---|
| `--netplay_host=PORT` | Host as player 1 on this UDP port. The game waits at boot until player 2 connects |
| `--netplay_join=ADDRESS:PORT` | Join as player 2 |
| `--netplay_delay=N` | Input delay in ticks (default 3); the host's value is used |
| `--netplay_pad=N` | Which local controller (0–3) this machine's player uses (default 0) |
| `--netplay_timeout=MS` | Give up and continue offline after this long without packets (default 10000) |
| `--netplay_sim_latency=MS`, `--netplay_sim_loss=PCT` | Developer: delay or drop outgoing packets, to simulate the internet locally |

The log reports every 30 s: `netplay: ticks A-B: N stalls (X ms waiting), F fingerprints
compared, in sync`. Frequent stalls mean the delay is too low for the connection.

## Tests

0. **Replay regression.** Build, then replay one of the existing recordings (format version 2
   is unchanged). It must still match with 0 drifting ticks. The replay code moved, so check
   this before anything relies on it.
1. **Precondition: does the replay hook override everything?** Replay a recording while
   pressing buttons and moving the sticks on the real controller.
   - It must still match with 0 drifts. Netplay assumes the game's only view of the controllers
     is the buffer that `sub_821135C0` fills (`game::kControllerStates`).
   - If live input leaks in (a drift that follows your presses), find where. For example,
     `sub_821135C0` might keep its own history from the raw XInput read. Report it before
     going further: netplay would then need to hook lower, at the raw controller read.
2. **Two copies on one PC.** Two controllers, two copies of the game started from the same
   folder (they read the same save):
   - A: `--netplay_host=7000`
   - B: `--netplay_join=127.0.0.1:7000 --netplay_pad=1`
   - Check whether each window still reads controllers while it isn't focused. If the SDK
     only reads input in the focused window, note it, since netplay on one PC needs both.
   - Play from the title screen: either player can press Start. Play a Training fight and an
     Arcade or versus stage to the result and on to the next.
   - Check that the log shows the 30 s reports with **in sync** and no desync errors.
   - Check that each window shows its own robot's view in fights.
3. **Simulated internet.** The same, with `--netplay_sim_latency=40 --netplay_sim_loss=2` on
   both. Try delays 3, 4 and 5 and note which feels playable and what the stall reports say.
4. **Two PCs**, on a LAN if available. Then over the internet with port forwarding or a VPN
   (ZeroTier, Tailscale), if the owner wants to.
5. **Disconnect.** Close one window mid-fight. The other should log that the other player
   stopped answering after `netplay_timeout` and continue offline, with the opponent back
   under CPU control or idle.

## If they desync

The first desync line gives the tick and regions. Diff the two `netplay_tick<N>_*.bin` files,
which have the same layout as replay dumps. Likely causes:

- **Different save data or settings** on the two machines. Copy the save folder; check every
  setting that could affect gameplay.
- **Boot timing.** Replays showed part of boot waits a number of vblanks, not ticks. If a
  machine stalls on the network during boot, those waits can end on different ticks. A desync
  early in boot, right after a stall, points here. Possible fixes: a larger delay or a pause
  before boot proceeds, or later making those waits tick-based (a decomp target).
- **Live input leaking in:** test 1.
- **Something machine-specific** reaching game state: time, frame timing, renderer read-backs.

## Known limits of the prototype

- **Setup:** command line only, IPv4 only, and the host waits forever for a joiner.
- **Connection:** no reconnection after a drop, no spectators.
- **Lockstep:** the game waits for late input instead of predicting it (rollback is a later
  upgrade).
- **Controllers:** slots 2 and 3 are zero in the shared input. Check nothing reacts to that,
  such as a "controller disconnected" prompt.

## Report back

- Results of tests 0–5. For test 3, the delay and the stall reports at the simulated latency.
- Any desync: tick, regions, cause, fix.
- Whether live input leaks into replays (test 1).
- Whether an F1 Netplay panel (host or join, address, delay) should come next.
