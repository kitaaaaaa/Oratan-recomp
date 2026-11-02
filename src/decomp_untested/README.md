# Untested rewrites (not built)

Hand-decompiled functions that were written but could not be checked, because the reference
replay (`tests/replays/feiyen_training.otr`: Fei-Yen against Angelan in Training, one stage) never
or almost never runs them. Nothing here is in `CMakeLists.txt`. To use one, move it next to the
checked code (`src/decomp/`), add it to the build, and check it as described in
`notes/HANDOFF-decomp.md`.

| File | Function | State |
|---|---|---|
| `robot_state26.cpp` | `sub_821F5258`, movement state 26 (the robots' special moves) | complete file with hook and verifier; builds; never runs in the replay |
| `robot_move_number_busy.cpp` | `sub_82242528`, the move number while Game +24932 is set | complete file with hook and verifier; builds; the replay never sets Game +24932 |
| `to_state11_fragment.cpp` | `sub_821DD810`, to state 11 | function body only; belongs in `src/decomp/robot_states.cpp`, hooked through `VerifySmall` like the other small helpers |
| `air_attack_again_fragment.cpp` | `sub_821DEC58`, whether an air attack can fire again | function body only; same as above; ran about twice in the replay |
| `turned_angles_fragment.cpp` | `sub_821C01D8`, the robot's angles turned about the vertical axis | function body only; same as above; needs `sub_822A7398`, `sub_822A76A8`, `sub_822A7668` externs and `<climits>` |
