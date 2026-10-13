# frontend-walk-window

Same key timeline as frontend-walk, but start_mode 0 (windowed). Owner diagnosis 2026-10-13: a windowed DDrawCompat surface is composed by DWM, so the screen grab cannot be stale; together with the game's read-byte counter this separates "input ignored" from "capture frozen".
