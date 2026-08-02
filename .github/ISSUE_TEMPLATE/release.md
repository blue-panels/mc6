---
name: Release
about: Steps for making a release
title: 'Release vX.Y.Z'
labels: infra
---

- [ ] Rename the open milestone to `vX.Y.Z`.

- [ ] Actions -> **release-notes** -> **Run workflow**: milestone `vX.Y.Z`, all three
      boxes clear.

- [ ] Read the notes on the Summary page of that run: every merged pull request
      of the milestone is there, the wording is right, and the short summary
      says what the release is. The same text is in the `release-notes`
      artifact.

- [ ] Actions -> **release-notes** -> **Run workflow**: milestone `vX.Y.Z`, tick
      **Also commit the CHANGELOG.md section to the default branch**.

- [ ] Create the release tag:

      git checkout master && git pull --ff-only
      git tag -a vX.Y.Z -m "vX.Y.Z"
      git push origin vX.Y.Z

- [ ] Actions -> **release-notes** -> **Run workflow**: milestone `vX.Y.Z`, tick
      **Also publish: notes into the release, summary onto the milestone** and
      **Also publish the page to the wiki**.

- [ ] Releases -> the `vX.Y.Z` draft -> **Publish release**.
