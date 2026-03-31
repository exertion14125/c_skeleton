#ifndef __SKELETON_CONST_H__
#define __SKELETON_CONST_H__

#ifndef DEBUG // Release build
#define DEF_LOG_INI_PATH        "/data/conf/log/"
#else
#define DEF_LOG_INI_PATH        "../../../conf/log/"
#endif /* DEBUG */

#define DEF_LOG_UI_UDS_PATH             "/var/run/"

#endif /* __SKELETON_CONST_H__ */