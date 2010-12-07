all: \
    Example.app/Contents/MacOS/example \
    Example.app/Contents/Info.plist \
    Example.app/Contents/Resources/MainMenu.nib

Example.app/Contents/MacOS/example: example.clay Example.app/Contents/MacOS
	clay -I ../../../lib-clay -framework Cocoa -o Example.app/Contents/MacOS/example example.clay

Example.app:
	mkdir -p Example.app

Example.app/Contents: Example.app
	mkdir -p Example.app/Contents

Example.app/Contents/MacOS: Example.app/Contents
	mkdir -p Example.app/Contents/MacOS

Example.app/Contents/Resources: Example.app/Contents
	mkdir -p Example.app/Contents/Resources

Example.app/Contents/Info.plist: Info.plist Example.app/Contents
	cp Info.plist Example.app/Contents/

Example.app/Contents/Resources/MainMenu.nib: MainMenu.nib Example.app/Contents/Resources
	cp -a MainMenu.nib Example.app/Contents/Resources

clean:
	rm -rf Example.app
