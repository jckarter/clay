# Maintainer: agemogolk (agemogolk@github)

pkgname=clay-git
pkgver=20130115
pkgrel=1
pkgdesc='Clay is a programming language designed for Generic Programming.'
arch=('i686' 'x86_64')
url='http://claylabs.com/clay/'
license=('BSD')
depends=('llvm=3.2' 'clang=3.2') #'python' 'ocaml' really needed?
optdepends=()
makedepends=('git' 'cmake>=2.6')
provides=('clay')
conflicts=('clay-hg')

_gitroot='git://github.com/jckarter/clay.git'
_gitname='clay'

build() {
    cd "$srcdir"
    msg "Connecting to GIT server...."

    if [[ -d "$_gitname" ]]; then
        cd "$_gitname" && git pull origin
        msg "The local files are updated."
    else
        git clone "$_gitroot" "$_gitname"
    fi

    msg "GIT checkout done or server timeout"
    msg "Starting build..."

    cd "$srcdir/$_gitname"

    rm -Rf build
    mkdir build
    cd build
    cmake -G "Unix Makefiles" ../ -DCMAKE_INSTALL_PREFIX=/opt/clay -DCMAKE_BUILD_TYPE=Release
    
    make VERBOSE=0
}

package() {
    cd "$srcdir/$_gitname/build"
    make DESTDIR=${pkgdir} install

    install -dm755 ${pkgdir}/usr/bin
    cd ${pkgdir}/usr/bin
    ln -s /opt/clay/bin/clay .
    ln -s /opt/clay/bin/claydoc .
    ln -s /opt/clay/bin/clay-bindgen .
}