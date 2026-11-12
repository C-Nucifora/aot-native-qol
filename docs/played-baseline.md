# Played baseline and rollback

The last played baseline was recorded during the 12 November 2026 nape retest.
Its core artifact SHA256 was `3257c6ec77bbade4cf40d200a227c4652331001c87ee9d33dc4ecbb483d806fc`.
The source-only core package was rebuilt locally with identical PE sections and
entrypoint to that artifact. PE timestamps can differ on later builds.

The original installation also used the focus module, getter-only observer,
external vkBasalt SMAA, and the DXVK FPS counter. Unlimited was selected.
Native SMAA and the HD heading adapter were added afterward and have been removed
from the restored installation. The later bound correction is also reverted.
Current saves must always be preserved when rolling back code or display settings.

The backed-up registry display choices were Resolution 10 and WindowMode 0.
They are build-specific indices, not a portable instruction to edit another
installation. No user registry or saved game is included in this repository.
