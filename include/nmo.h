#ifdef __cplusplus
extern "C" {
#endif

void nmo_start(const char *tag);
void nmo_stop();
void nmo_tag_addr(const char *tag, void *start, void *end);
void nmo_tag_from_maps(const char *tag, const char *file);

#ifdef __cplusplus
}
#endif
