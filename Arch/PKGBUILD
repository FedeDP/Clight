# Maintainer: Federico Di Pierro <nierro92@gmail.com>

pkgname=clight-git
_gitname=Clight
pkgver=r508.b62c77b
pkgrel=1
pkgdesc="A C daemon that turns your webcam into a light sensor. It can also change display gamma temperature, dim your screen and set your dpms. Devel version."
arch=('i686' 'x86_64')
url="https://github.com/FedeDP/${_gitname}"
license=('GPL')
backup=(etc/default/clight.conf)
depends=('systemd>=221' 'popt' 'libconfig' 'gsl' 'clightd-git')
makedepends=('git' 'cmake' 'bash-completion')
optdepends=('geoclue2: to retrieve user location through geoclue2.'
            'upower: to save energy by increasing timeouts between captures while on battery and to autocalibrate keyboard backlight.'
            'bash-completion: to add support for bash automatic completion.')
source=("git://github.com/FedeDP/${_gitname}.git")
install=clight.install
sha256sums=("SKIP")

pkgver() {
    cd "$_gitname"
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

prepare() {
    cd "${srcdir}/${_gitname}"
    mkdir -p build
}

build() {
    cd "${srcdir}/${_gitname}/build"
    cmake \
        -G "Unix Makefiles" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_BUILD_TYPE="Release" \
        ..
    make
}

package() {
    cd "${srcdir}/${_gitname}/build"
    make DESTDIR="$pkgdir" install
}
