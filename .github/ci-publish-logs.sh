#!/bin/bash
# Push the files in /tmp/logs to a branch of this repository, so build problems can be read without opening the web UI.
branch="$1"
rm -rf /tmp/logrepo && mkdir /tmp/logrepo && cd /tmp/logrepo || exit 0
git init -q && git checkout -q -b "$branch"
cp /tmp/logs/* . 2>/dev/null || echo "no logs" > none.txt
git add -A
git -c user.name=ci -c user.email=ci@example.com commit -qm "logs for ${GITHUB_SHA}"
git push -f "https://x-access-token:${GITHUB_TOKEN}@github.com/${GITHUB_REPOSITORY}.git" "$branch"
