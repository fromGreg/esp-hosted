/* Linux boot Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const void *map_partition_part(const char *name, uint32_t size, bool check)
{
	const void *ptr;
	spi_flash_mmap_handle_t handle;
	esp_partition_iterator_t it;
	const esp_partition_t *part;
	const uint32_t mask = 0x01ffffff;

	it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, name);
	part = esp_partition_get(it);
	if (esp_partition_mmap(part, 0, size ? size : part->size,
			       SPI_FLASH_MMAP_INST, &ptr, &handle) != ESP_OK)
		abort();
	if (check && ((uint32_t)ptr & mask) != (part->address & mask)) {
		printf("mapping %s: expected: 0x%08x, actual: %p\n",
		       name, part->address & mask, ptr);
		abort();
	}

	return ptr;
}

static const void *map_partition(const char *name)
{
	return map_partition_part(name, 0, true);
}

static void map_psram_to_iram(void)
{
	uint32_t *dst = (uint32_t *)DR_REG_MMU_TABLE + 0x100;
	uint32_t *src = (uint32_t *)DR_REG_MMU_TABLE + 0x180;
	int i;

	for (i = 0; i < 0x80; ++i) {
		dst[i] = src[i];
	}
}

static void cache_partition(const char *name)
{
	esp_partition_iterator_t it;
	const esp_partition_t *part;
	char v;

	it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, name);
	part = esp_partition_get(it);
	if (esp_partition_read(part, 0, &v, 1) != ESP_OK)
		abort();
}

static char IRAM_ATTR space_for_vectors[4096] __attribute__((aligned(4096)));

static void map_flash_and_go(void)
{
	const void *ptr;

	/* Align mapping address with partition address */
	map_partition_part("factory", 0x40000, false);

	map_partition("etc");
	ptr = map_partition("linux");
	printf("linux ptr = %p\n", ptr);
	printf("vectors ptr = %p\n", space_for_vectors);
	map_partition("rootfs");

	map_psram_to_iram();

	cache_partition("nvs");

	extern int g_abort_on_ipc;
	g_abort_on_ipc = 1;

	asm volatile ("jx %0" :: "r"(ptr));
}

static void linux_task(void *p)
{
	map_flash_and_go();
	esp_restart();
}

void linux_boot(void)
{
	xTaskCreatePinnedToCore(linux_task, "linux_task", 4096, NULL, 5, NULL, 1);
}
