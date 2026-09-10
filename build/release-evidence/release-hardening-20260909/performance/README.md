# Heavy-enemy rendering check

Diagnostic runs from the repaired source before the Alpha 5 candidate build.
Each scene contains the maximum four active enemies, all armor type and armor 4,
on original stages 1 and 26. Stable surviving IDs select only German Tiger,
American T28/T95, or Soviet KV-5 models under the production nation rules.
The player/camera and enemies occupy nearby legal positions in the map center.

1280x720 logical / 2560x1440 Retina; Pixel Style off; normal high shadows;
60 warmup frames and five measured seconds per scene. Physics was frozen for
repeatable scene content; wall-clock animation, input polling, actual rendering
and normal frame pacing continued. See scenes.csv for exact camera/terrain data.

Four American T95 enemies measured 84.0 FPS on stage 1 and 61.8 FPS on stage 26,
versus 85.8 and 59.4 FPS for the German reference. This short sequential sample
did not show a substantial additional American-roster rendering cost. It does
not prove a sustained frame-rate guarantee. The first German scene had a 29.2
FPS 1% low, below the formal 30 FPS long-session threshold; all raw frames are
retained. The original profile helper adds timers around real rendering calls,
so these are diagnostic observations, not candidate-generated formal telemetry
or a replacement for the required 30-minute focused gameplay session.

The helper sources and their source hashes are retained for reproduction.
They originate at build/enemy-performance-20260909; prepare_profile.py reads
current production source, instruments render timing, and compiles native_profile.
