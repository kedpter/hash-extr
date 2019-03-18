# header
find hashcat/include -type f -name '*.h' -exec sed -E -i .old '/#define (_[A-Z]+)+_H/a \
#ifdef __cplusplus \
extern "C" { \
#endif \
' {} \;

# tail
find hashcat/include -type f -name '*.h' -exec sed -E -i .old '/#endif.*\/\/.*(_[A-Z]+)+_H/i \
  #ifdef __cplusplus \
} \
#endif \
' {} \;

rm hashcat/include/*.old
