[English](CONTRIBUTING.md) | [Русский](CONTRIBUTING_RU.md)

# Contributing

Keep upstream YCJ-A002 addresses/enums stable. Document every extension in docs/registers.csv and docs/REGISTERS.md. Run `python tools/test_host.py` and compile `haier-s3.yaml` before submitting changes. Never include secrets.yaml, factory firmware, personal chat/agent notes, private logs or local toolchains. Clearly separate hardware observations from simulated results. Preserve licenses and the MONITOR_ONLY guard. Do not change eFuses as part of setup or tests.

## Documentation languages

English is the default: README.md and unsuffixed guides. Each has a Russian `_RU.md` counterpart. Update both languages in the same change, including tables, examples, limitations and links. Keep protocol identifiers, register values and commands identical. Keep language navigation on every page; maintain both schematic language variants through tools/draw_schematic.py. Run `python tools/check_docs.py` after documentation edits.
