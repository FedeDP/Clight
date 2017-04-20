# Maintainer: Federico Di Pierro <nierro92@gmail.com>

pkgname=clight-git
_gitname=Clight
pkgver=r78.daf725b
pkgrel=1
pkgdesc="A C daemon that turns your webcam into a light sensor. It can also change display gamma temperature."
arch=('i686' 'x86_64')
url="https://github.com/FedeDP/${_gitname}"
license=('GPL')
depends=('systemd' 'libxcb' 'popt' 'libconfig' 'clightd-git')
makedepends=('git')
optdepends=('geoclue2: to retrieve user location through geoclue2.')
source=("git://github.com/FedeDP/${_gitname}.git")
install=clight.install
sha256sums=("SKIP")

pkgver() {
    cd "$_gitname"
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cd $srcdir/$_gitname
    make
}

package() {
    cd $srcdir/$_gitname
    make DESTDIR="$pkgdir" install
}
