#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Configurações de Wi-Fi
#define WIFI_SSID "Internet"
#define WIFI_PASSWORD "Regeddit"

// Definições dos botões
#define BOTAO_A 5
#define BOTAO_B 6

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Leitura da temperatura interna
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;

    // Leitura dos botões (inverso pois pull-up)
    int estado_a = !gpio_get(BOTAO_A);
    int estado_b = !gpio_get(BOTAO_B);

    // Cria a resposta HTML com temperatura e botões
    char html[1024];
    snprintf(html, sizeof(html),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/html\r\n"
         "\r\n"
         "<!DOCTYPE html>\n"
         "<html>\n"
         "<head>\n"
         "<meta charset=\"UTF-8\">\n"
         "<script>setTimeout(() => location.reload(), 1000);</script>"
         "<title>Status do Sistema</title>\n"
         "<style>\n"
         "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
         ".valor { font-size: 32px; margin: 10px 0; }\n"
         "</style>\n"
         "</head>\n"
         "<body>\n"
         "<h1>Status do Sistema</h1>\n"
         "<div class=\"valor\">Temperatura: %.2f &deg;C</div>\n"
         "<div class=\"valor\">Botão A: %s</div>\n"
         "<div class=\"valor\">Botão B: %s</div>\n"
         "</body>\n"
         "</html>\n",
         temperature,
         estado_a ? "PRESSIONADO" : "SOLTO",
         estado_b ? "PRESSIONADO" : "SOLTO");


    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função principal
int main()
{
    stdio_init_all();

    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Inicializa botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    // Inicializa ADC para leitura da temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    while (true)
    {
        cyw43_arch_poll(); // mantém Wi-Fi e TCP funcionando
    }

    cyw43_arch_deinit();
    return 0;
}
