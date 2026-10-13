# Architecture

T0007 adds shared Trace integrity validation at parsing and comparison boundaries. Invalid comparisons have explicit diagnostics and cannot pass. Sparse schedules and differing adapter identities/provenance remain supported. Snapshot ticks use the existing scenario-relative completed-tick range 0..header.ticks.
