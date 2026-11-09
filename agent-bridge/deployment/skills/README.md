# Packaged skill snapshot

These four skill files are build inputs for the standalone FrameLab package.
The desktop repository versions them with the runtime, so a build does not
require an unpublished neighboring ai-skills checkout.

`SNAPSHOT.json` records the source checkout baseline and SHA-256 of each copied
file. The original source files were untracked at capture time. Their complete
contents are preserved here and become pinned by this desktop commit.

To test a separately maintained skill revision, pass `--skill-dir` to
`deployment/build.mjs`. The selected directory must have the sibling
`thorvision-analytics` compatibility skill. Release provenance records that
override as external and includes its file hashes.
