# setup environment variables for building OpenSCAD against custom built
# dependency libraries. called by linux-build-dependencies.sh

# run this file with 'source setenv-linbuild.sh' every time you re-login
# and want to build or run openscad against custom libraries installed
# into BASEDIR.  

# copy this file to your .bashrc if desired. 

if [ ! $BASEDIR ]; then
  BASEDIR=$HOME/openscad_deps
fi
DEPLOYDIR=$BASEDIR

export PATH=$BASEDIR/bin:$PATH
export LD_LIBRARY_PATH=$DEPLOYDIR/lib:$DEPLOYDIR/lib64
export LD_RUN_PATH=$DEPLOYDIR/lib:$DEPLOYDIR/lib64
export OPENSCAD_LIBRARIES=$DEPLOYDIR
export GLEWDIR=$DEPLOYDIR

echo BASEDIR: $BASEDIR
echo DEPLOYDIR: $DEPLOYDIR
echo PATH modified
echo LD_LIBRARY_PATH modified
echo LD_RUN_PATH modified
echo OPENSCAD_LIBRARIES modified
echo GLEWDIR modified

if [ "`uname -a | grep ppc`" ]; then
	# workaround for https://bugs.freedesktop.org/show_bug.cgi?id=42540
	export GALLIUM_DRIVER=softpipe
	export DRAW_USE_LLVM=n
	echo "detected ppc.."
	echo "GALLIUM_DRIVER modified"
	echo "DRAW_USE_LLVM modified"
fi

if [ "`command -v qmake-qt4`" ]; then
	echo "Please re-run qmake-qt4 and run 'make clean' if necessary"
else
	echo "Please re-run qmake and run 'make clean' if necessary"
fi

