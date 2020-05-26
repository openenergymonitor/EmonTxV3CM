Import("env", "projenv")
from shutil import copyfile

def save_hex(*args, **kwargs):
    print("Copying hex output to project directory...")
    target = str(kwargs['target'][0])
    copyfile(target, 'output.hex')
    print("Done.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", save_hex)   #post action for the target hex 
