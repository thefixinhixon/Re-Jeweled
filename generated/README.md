# generated/

Filled by `rexglue codegen bejeweled_manifest.toml` from your own
`assets/default.xex`, then post-processed by `scripts/refix.sh`.

This tree **is** committed so the Windows CI job can compile `bejeweled.exe`
without the game's executable in the checkout. It contains no game data -
only machine-translated C++ derived from `default.xex`. After a fresh
codegen, run `bash scripts/refix.sh` and commit the result.
