#pragma once

struct order_data;

void db_driver_open(void);
void db_driver_close(void);
int db_driver_order_save(struct order_data *od);
struct order_data *db_driver_order_load(int id);
