#ifndef BACKGROUND_H_H
#define BACKGROUND_H_H

void background_login(qq_account* ac);
void background_friends_info(qq_account* ac);
void background_msg_poll(qq_account* ac);
void background_msg_drain(qq_account* ac);
void background_group_detail(qq_account* ac,LwqqGroup* group);

const char* translate_smile(int face);


#endif
