# Pointing mode keys

In Entropy's custom key picker, assign these actions to change the persistent
**Sticky mode / Фиксация режима** setting from the keyboard:

| Action | Behavior |
| --- | --- |
| `EH_STK_TG` | Toggle sticky mode for all pointing devices |
| `EH_STK_ON` | Enable sticky mode for all pointing devices |
| `EH_STK_OFF` | Disable sticky mode and clear the current mode-key selection |
| `EH_L_STK_TG`, `EH_L_STK_ON`, `EH_L_STK_OFF` | The same actions for the left device |
| `EH_R_STK_TG`, `EH_R_STK_ON`, `EH_R_STK_OFF` | The same actions for the right device |

The left/right actions are available on boards with independent pointing-device
settings. If the two sides differ, the shared toggle enables both; the next press
disables both. Actions run on key press. Releasing a key does not change the setting.
The setting uses the same storage as configurator edits and survives a restart.
Turning sticky mode off clears the selected override; split devices return to
their configured pointer mode. Existing `EH_SNP`, `EH_SCR`, and `EH_TXT` assignments
keep their tap/hold behavior.

These actions control pointer-mode fixation, not the auto layer. To leave an
active auto layer immediately, put `TO(0)` on that layer. `MO(0)` leaves higher
layers active. Further pointer activity can activate the auto layer again.
