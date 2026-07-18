AXSound assets are intentionally small.

Music is just a longer/looping .axsound.
Gameplay code decides when to play/stop/change sounds.

Example:
  ax::sound::runtime().play("assets/audio/SwordHit.axsound", position);

Drop real .wav/.ogg files next to these metadata files when an audio backend is added.
The current Stage 3.5 runtime records/prints sound events so the gameplay wiring is testable now.
