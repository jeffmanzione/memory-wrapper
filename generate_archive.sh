PROJECT="memory-wrapper"
VERSION="0.1.0"

git archive \
    --format=zip \
    --prefix "$PROJECT-$VERSION/" \
    --output "./$PROJECT-$VERSION.zip" \
    master

git archive \
    --format="tar.gz" \
    --prefix "$PROJECT-$VERSION/" \
    --output "./$PROJECT-$VERSION.tar.gz" \
    master