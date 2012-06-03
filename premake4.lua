solution "clay"
    configurations { "Debug", "Release" }
    configuration "Debug"
        flags { "Symbols" }

    configuration "Release"
        flags { "Optimize" }


    -- llvm

    newoption {
        trigger = "with-llvm-config",
        value = "path",
        description = "Specify path to llvm-config"
    }
    local llvmConfig = io.popen((_OPTIONS["with-llvm-config"] or "llvm-config") .. " --ldflags", 'r' );
    local llvmLdFlags = llvmConfig:read('*a'):gsub("\n","");
    llvmConfig = io.popen((_OPTIONS["with-llvm-config"] or "llvm-config") .. " --libs", 'r' );
    local llvmLibs  =  llvmConfig:read('*a'):gsub("\n",""):gsub("-l",""):explode(" ")
    llvmConfig = io.popen((_OPTIONS["with-llvm-config"] or "llvm-config") .. " --cxxflags", 'r' );
    local llvmCxxFlags = llvmConfig:read('*a'):gsub("\n","");


    -- clay

    project "clay"
        kind "ConsoleApp"
        language "C++"
        files { "compiler/src/*.cpp", "compiler/src/*.hpp" }
        linkoptions  { llvmLdFlags }
        buildoptions { llvmCxxFlags }
        links        { llvmLibs }

        if (os.get() == "linux") then
            links { "rt" }
        end


    -- claydoc

    project "claydoc"
        kind "ConsoleApp"
        language "C++"
        files { "compiler/src/*.cpp", "compiler/src/*.hpp" }
        excludes "compiler/src/main.cpp"
        files { "tools/claydoc/*.cpp" }
        linkoptions  { llvmLdFlags }
        buildoptions { llvmCxxFlags }
        links        { llvmLibs }

        if (os.get() == "linux") then
            links { "rt" }
        end



