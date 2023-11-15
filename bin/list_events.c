#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <perfmon/pfmlib.h>

void errx(int x, const char *s)
{
   fprintf(stderr, "%s\n", s);
   exit(1);
}

void list_pmu_events(pfm_pmu_t pmu)
{
   pfm_event_info_t info;
   pfm_pmu_info_t pinfo;
   pfm_event_attr_info_t ainfo;
   int i, ret;

   memset(&info, 0, sizeof(info));
   memset(&pinfo, 0, sizeof(pinfo));
   memset(&ainfo, 0, sizeof(ainfo));

   info.size = sizeof(info);
   pinfo.size = sizeof(pinfo);
   ainfo.size = sizeof(ainfo);

   ret = pfm_get_pmu_info(pmu, &pinfo);
   if (ret != PFM_SUCCESS)
      //errx(1, "cannot get pmu info");
      return;

   for (i = pinfo.first_event; i != -1; i = pfm_get_event_next(i)) {
      ret = pfm_get_event_info(i, PFM_OS_PERF_EVENT_EXT, &info);
      if (ret != PFM_SUCCESS)
        errx(1, "cannot get event info");
      else if (pinfo.is_present) {
        printf("%s::%s [%s]\n", pinfo.name, info.name, info.desc);
        for (int j = 0; j < info.nattrs; j++) {
           pfm_get_event_attr_info(i, j, PFM_OS_PERF_EVENT_EXT, &ainfo);
           printf("\t%s (%s) [%s]\n", ainfo.name, ainfo.type == PFM_ATTR_UMASK ? "umask" : ainfo.type == PFM_ATTR_MOD_BOOL ? "bool" : "int", ainfo.desc);
        }
      }
  }
}

int main()
{
    pfm_initialize();
    pfm_pmu_t p;
    pfm_for_all_pmus(p) {
       list_pmu_events(p);
    }
}
