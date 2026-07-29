# Packaging mc6

`mc6` is the package name for this fork.  It still installs `mc`, `mcedit`,
`mcview`, `mcdiff`, and `mctree`; it cannot coexist with the distribution
package that owns the same paths.

The recipes use an explicit migration.  A routine system update never changes
the installed `mc` to this fork.

## Nothing to edit for a release

No recipe here names a version.  The version comes from the release tag, and
`packaging/prepare.sh` writes the files that have to name one:

| Generated | From |
| --- | --- |
| `debian/changelog` | the `v*` tags |
| `packaging/rpm/mc6.spec` | `mc6.spec.in` and the `v*` tags |
| `packaging/arch/PKGBUILD` | `PKGBUILD.in` |
| `packaging/gentoo/mc6-<version>.ebuild` | `mc6.ebuild.in` |

All four are in `.gitignore`.  Making a release is therefore: tag, and let the
`release` workflow build.  Nothing is committed afterwards.

Both changelogs take their entries from the **annotated tag message** and their
dates from the tag, so the same tag always yields the same source package.
What a release says is written once, when the tag is signed:

```sh
git tag -s v6.0.3 -m "Filtered view in mcview.
Faster paste into the editor."
```

A version with no tag of its own is a test build.  Its Debian entry is marked
`UNRELEASED`, which stops `dput` from taking it by accident.

The build dependency lists intentionally enable every shipped panel plugin.
Do not replace their exact core-version dependencies with `>=`: the plugin ABI
is not stable.

## Release source

`packaging/release-source.sh` creates the archive that every recipe consumes,
and that is uploaded as the GitHub release asset `mc6-<version>.tar.gz`:

```sh
packaging/release-source.sh 6.0.3 v6.0.3
```

It adds the generated `mc-version.h`, so a build outside a Git checkout reports
the correct fork version, and it refuses to overwrite an existing archive.  The
output is reproducible: the same tag always gives the same bytes, hence the same
checksum.  Every recipe runs autotools again, because Git archives do not
contain the generated `configure` files.

`debian/` and `packaging/` carry `export-ignore` in `.gitattributes` and are
absent from the archive: the recipes are used from the repository, and a Debian
orig tarball must not carry a `debian` directory of its own.

## Building by hand

The `release` workflow does all of this on a tag push, in a container per
distribution.  Locally the same steps check that a distribution can still build
the package.

Debian and Ubuntu.  The source package is built from the release archive with
`debian/` copied into it, not from the Git checkout: that keeps it free of
patches, since the archive carries `mc-version.h` and no `debian` directory.

```sh
packaging/release-source.sh 6.0.3 v6.0.3
packaging/prepare.sh 6.0.3 dist/mc6-6.0.3.tar.gz
tar -xf dist/mc6-6.0.3.tar.gz -C /tmp
cp dist/mc6-6.0.3.tar.gz /tmp/mc6_6.0.3.orig.tar.gz
cp -r debian /tmp/mc6-6.0.3/
cd /tmp/mc6-6.0.3 && dpkg-buildpackage -us -uc
```

`sudo apt install ../mc6_*.deb ../mc6-data_*.deb ../mc6-plugins_*.deb` installs
the result; use `apt install ./file.deb`, never `dpkg -i`.  For a repository or
PPA users install `mc6` and `mc6-plugins` with apt; the transition removes `mc`
and `mc-data`, and `sudo apt purge mc mc-data` later clears old conffile
records.  For a PPA, set the target series and a series suffix:

```sh
DEB_DISTRIBUTION=noble DEB_VERSION_SUFFIX='~ubuntu24.04.1' \
    packaging/prepare.sh 6.0.3 dist/mc6-6.0.3.tar.gz
```

RPM.  Put the archive in the RPM source directory:

```sh
packaging/prepare.sh 6.0.3 dist/mc6-6.0.3.tar.gz
cp dist/mc6-6.0.3.tar.gz ~/rpmbuild/SOURCES/
rpmbuild -ba packaging/rpm/mc6.spec
```

`Conflicts` is deliberately used instead of `Obsoletes`, so `dnf upgrade` does
not silently replace a distribution package.  The repository instruction is
`sudo dnf swap mc mc6` followed by `sudo dnf install mc6-plugins`; for a
downloaded RPM use `dnf install ./mc6-*.rpm`, never `rpm -i`.

Arch.  `makepkg` finds the archive next to the `PKGBUILD` instead of
downloading it:

```sh
packaging/prepare.sh 6.0.3 dist/mc6-6.0.3.tar.gz
cp dist/mc6-6.0.3.tar.gz packaging/arch/
cd packaging/arch && makepkg -si
```

From a repository or the AUR the same two names are installed with
`sudo pacman -S mc6 mc6-plugins`.

Gentoo.  Copy `packaging/gentoo` into a personal overlay as `app-misc/mc6`,
together with the generated ebuild, and run `ebuild ... manifest`.  It is a
single package; panel plugins are controlled by USE flags, and the strong
blocker requires an explicit migration from `app-misc/mc`:

```sh
emerge app-misc/mc6
```

Gentoo is the one distribution the workflow does not build: that needs a full
stage3 with a Portage snapshot, which costs more than the ebuild is worth.  It
stays a manual check.
