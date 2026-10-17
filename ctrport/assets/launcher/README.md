# Launcher logo

`logo.png` is the user-supplied launcher branding, with its white background removed using the built-in imagegen tool on 2026-10-16. It was not extracted from a retail dump. Original input: `codex-clipboard-d84c223a-4e66-4a7b-9b77-0070f3ba50d1.png`. This artwork is not covered by the source code's GPL license; existing marks remain their owners' property.

Edit prompt:

> Edit target: the supplied CTR CRASH TEAM RACING NATIVE >> PORTED logo. Remove ONLY the white background and the white negative spaces between and inside the letters, making them truly transparent alpha. Preserve the exact original lettering, layout, colors, outlines, ribbon, metallic shading, proportions and fine details; do not redesign, reword or add anything. Preserve the white letters on the red ribbon and all intentional white highlights. Clean antialiased silhouette, no white matte or halo. Keep the complete logo uncropped, tightly framed with a small transparent margin. Output a transparent PNG for a desktop launcher.

The build script embeds this asset in the executable as resource 101 and also copies it to `assets/launcher/logo.png` beside the executable. Include that relative path when packaging the launcher. Source-tree execution also supports this source asset path. If the external file is missing, the launcher uses the embedded logo; the text heading is only an emergency fallback if no logo can be decoded. Retail Home artwork is loaded directly from the user's dump and must never be included in this directory.
