__all__ = ["installGlobalsCleanupHook", "cleanupGlobals"]


_globalsCleanupHooks = []

def installGlobalsCleanupHook(f) :
    _globalsCleanupHooks.append(f)

def cleanupGlobals() :
    for f in reversed(_globalsCleanupHooks) :
        f()
