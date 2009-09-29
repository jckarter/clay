__all__ = ["installGlobalsCleanupHook", "cleanupGlobals", "isCleaningUp"]


_globalsCleanupHooks = []

def installGlobalsCleanupHook(f) :
    _globalsCleanupHooks.append(f)

_cleaningUp = False

def cleanupGlobals() :
    global _cleaningUp
    _cleaningUp = True
    for f in _globalsCleanupHooks :
        f()

def isCleaningUp() : 
    return _cleaningUp
