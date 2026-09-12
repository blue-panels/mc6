- [ ] Refresh the gettext template, a week before the tag so that translators
      have time. Run `./autogen.sh` to rebuild `po/POTFILES.in`, then in the
      build directory:

      make -C po mc.pot-update

      Commit `po/mc.pot` if it changed. Weblate reads it from master and puts
      the new strings into the po files by itself.

- [ ] Actions -> **l10n-pull** -> **Run workflow**: it brings the translations
      from Weblate into a pull request. Merge that pull request, do not squash
      it, the commits carry the names of the translators.

- [ ] Rename the open milestone to `vX.Y.Z`.

- [ ] Actions -> **Create release notes** -> **Run workflow**: milestone `vX.Y.Z`, all three
      boxes clear.

- [ ] Read the notes on the Summary page of that run: every merged pull request
      of the milestone is there, the wording is right, and the short summary
      says what the release is. The same text is in the `release-notes`
      artifact.

- [ ] Actions -> **Create release notes** -> **Run workflow**: milestone `vX.Y.Z`, tick
      **Also commit the CHANGELOG.md section to the default branch**.

- [ ] Create the release tag:

      git checkout master && git pull --ff-only
      git tag -a vX.Y.Z -m "vX.Y.Z"
      git push origin vX.Y.Z

- [ ] Actions -> **Create release notes** -> **Run workflow**: milestone `vX.Y.Z`, tick
      **Also publish: notes into the release, summary onto the milestone** and
      **Also publish the page to the wiki**.

- [ ] Releases -> the `vX.Y.Z` draft -> **Publish release**.
