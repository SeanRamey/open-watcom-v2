#!/bin/sh
# *****************************************************************
# gitupdf.sh - update git repository if build fails
# *****************************************************************
#
# after failure transfer log files back to GitHub repository
#

if [ "$OWTRAVIS_DEBUG" = "1" ]; then
    GITQUIET=-v
else
    GITQUIET=--quiet
fi

echo_msg="gitupdf.sh - skipped"

if [ "$TRAVIS_BRANCH" = "$OWBRANCH" ]; then
    if [ "$TRAVIS_EVENT_TYPE" = "push" ]; then
        if [ "$OWTRAVISJOB" = "BOOTSTRAP" ] || [ "$OWTRAVISJOB" = "BUILD" ] || [ "$OWTRAVISJOB" = "DOCPDF" ]; then
            cd $OWTRAVIS_BUILD_DIR
            #
            # remove all local changes to upload only error logs
            #
            git reset --hard
            git clean -fd
            #
            # copy build log files to git repository tree
            #
            if [ "$TRAVIS_OS_NAME" = "osx" ]; then
                if [ ! -d bld/osx ]; then mkdir -p logs/osx; fi
                cp $TRAVIS_BUILD_DIR/bld/*.log logs/osx/
            else
                if [ ! -d bld/linux ]; then mkdir -p logs/linux; fi
                cp $TRAVIS_BUILD_DIR/bld/*.log logs/linux/
            fi
            #
            # commit new log files to GitHub repository
            #
            git add -f .
            if [ "$TRAVIS_OS_NAME" = "osx" ]; then
                git commit $GITQUIET -m "Travis CI build $TRAVIS_JOB_NUMBER (failure) - log files (OSX)"
            else
                git commit $GITQUIET -m "Travis CI build $TRAVIS_JOB_NUMBER (failure) - log files (Linux)"
            fi
            git push $GITQUIET -f origin
            cd $TRAVIS_BUILD_DIR
            echo_msg="gitupdf.sh - done"
        fi
    fi
fi

echo "$echo_msg"
