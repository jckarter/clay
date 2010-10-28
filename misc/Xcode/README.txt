To add the Xcode templates, copy the contents of the "Project Templates" folder
to "~/Library/Application Support/Developer/Shared/Xcode/Project Templates".
If the "Project Templates" folder does not yet exist, you can create it. By
default, Clay files are set up to be built using the "clay-xcodebuild" script
installed in "/usr/local/bin". If you have Clay installed elsewhere, you will
need to modify the custom build rule on the primary target, or else symlink
"clay-xcodebuild" and "clay" into "/usr/local/bin".

KNOWN PROBLEMS
--------------
These project templates tell Xcode to build Clay files with a custom build rule
on the primary target. If you want to add new targets that use Clay, you will
need to add this custom build rule for those targets:

    Process:  Source files with names matching:  *.clay
    using:    Custom script:
      /usr/local/bin/clay-xcodebuild
    with output files:
      $(DERIVED_FILES_DIR)/$(INPUT_FILE_BASE).$(CURRENT_ARCH).s

Xcode does not do dependency analysis of Clay code, so if you change library
code, you will need to manually Touch your main.clay file to get Xcode to
rebuild it.

This whole approach is a hack. At some point we should write a proper Xcode
plugin for Clay, which will allow syntax highlighting and dependency analysis.
