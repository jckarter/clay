import glob, os

def post_run():
    [os.unlink(f) for f in glob.glob("temp*")]
