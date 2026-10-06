# Maintainers: Chris McGimpsey-Jones <chrisjones.unixmen@gmail.com>, Daniel Micay <danielmicay@gmail.com>
# Contributor: Mikael Eriksson <mikael_eriksson@miffe.org>
# Originally by: Denis Smirnov <detanator@gmail.com>

_pkgname=igt-gpu-tools
pkgname=intel-gpu-tools
pkgver=2.5
pkgrel=1
pkgdesc="Patched and repackaged tools for development and testing of the Intel DRM driver."
arch=(x86_64)
license=(MIT)
url='https://www.freedompublishersunion.net/h-linux.html'
depends=(libdrm libpciaccess cairo python xorg-xrandr pciutils libprocps kmod libxv libunwind peg systemd)
makedepends=(python-docutils swig xorg-util-macros xorgproto meson)
optdepends=('python-dissect.cstruct: for intel-gfx-fw-info')
#source=(https://xorg.freedesktop.org/releases/individual/app/${_pkgname}-$pkgver.tar.xz{,.sig})
sha512sums=('SKIP'
            'SKIP')

prepare() {
  mkdir -p build
  cd igt-gpu-tools-${pkgver}
  # Make man pages reproducible
  sed -i 's/gzip/gzip -n/' man/rst2man.sh
}

build() {
  cd build
  meson ../$_pkgname-$pkgver \
    --prefix=/usr \
    --libexecdir=/usr/lib

  ninja
}

check() {
  cd build
  ninja test
}

package() {
  cd build
  DESTDIR="$pkgdir" ninja install

  cd ../$_pkgname-$pkgver
  install -Dm644 COPYING "$pkgdir/usr/share/licenses/${pkgname}/COPYING"
}
