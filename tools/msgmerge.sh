cd kame
LIST=`find . -name \*.h -o -name \*.cpp`
if test -n "$LIST"; then
 xgettext -ki18n -kI18N_NOOP -ktr2i18n $LIST -o ../tools/kame.pot;
fi
cd ../../macosx/kame
LIST=`find . -name \*.h -o -name \*.cpp`
if test -n "$LIST"; then
 xgettext -ki18n -kI18N_NOOP -ktr2i18n $LIST -j -o ../../2.2/tools/kame.pot;
fi
cd ../../2.2/tools
msgmerge ../po/ja.po kame.pot -o ja.po
