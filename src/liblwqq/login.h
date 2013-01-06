/**
 * @file   login.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 02:25:51 2012
 *
 * @brief  Linux WebQQ Login API
 * 
 * 
 */

#ifndef LWQQ_LOGIN_H
#define LWQQ_LOGIN_H

#include "type.h"

/** 
 * WebQQ login function
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
void lwqq_login(LwqqClient *client, LwqqStatus status,LwqqErrorCode *err);

/** 
 * WebQQ logout function
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
void lwqq_logout(LwqqClient *client, LwqqErrorCode *err);


#endif  /* LWQQ_LOGIN_H */
