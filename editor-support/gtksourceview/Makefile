#stolen/adapted from http://github.com/nddrylliog/ooc-gtksourceview

install:
	mkdir -p ~/.local/share/gtksourceview-2.0/language-specs
	mkdir -p ~/.local/share/mime/packages
	cp -f clay.lang ~/.local/share/gtksourceview-2.0/language-specs
	cp -f clay.xml ~/.local/share/mime/packages
	cd ~/.local/share && update-mime-database mime

uninstall:
	rm ~/.local/share/gtksourceview-2.0/language-specs/clay.lang
	rm ~/.local/share/mime/packages/clay.xml
	cd ~/.local/share && update-mime-database mime
