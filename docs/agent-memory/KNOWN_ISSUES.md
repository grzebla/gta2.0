# Known Issues

T0004 at 959cbbce637fa46e81d9cf5a1aca1d35024b0088 removes the original checkout-wide deletion but remains unapproved. Self-test allocation failures can produce writes outside owned workspaces, and unchecked TMPDIR permits checkout-local allocations. Full mock-suite execution remains prohibited for agents.
