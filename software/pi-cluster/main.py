#!/usr/bin/env python3
"""
main.py (pi-cluster)
Cluster de instrumentos digital - renderiza sobre el panel ultrawide
(BOM item C.1, LESOWN M141-A01T - SOLO PROTOTIPO).

Programa: [Marca] - Defender Restomod
Documentos relacionados: DOC-PH1-ARQ-001 (Arquitectura), DOC-PH1-PROTO-001
(protocolo de datos)

Arquitectura de este archivo:
  - Un hilo de fondo lee el puerto serie de sensores (proto_reader.py) y
    actualiza cluster_state.py sin bloquear el render.
  - El heartbeat hacia Pi 2 corre en sus propios hilos (heartbeat.py).
  - El hilo principal es exclusivamente el loop de render de pygame -
    nunca debe esperar en I/O de puerto serie (por eso todo lo anterior
    corre en hilos aparte).

Uso: python3 main.py
"""

import sys
import threading

import pygame

import config
from proto_reader import ProtoReader
from cluster_state import ClusterState, CRITICAL_TIMEOUT_S
from heartbeat import HeartbeatLink
from proto_reader import NodeId

# ---------------------------------------------------------------------------
# Colores (paleta simple, alto contraste para legibilidad - la version de
# produccion con panel de 700-1000 nits ajustara brillo/contraste real)
# ---------------------------------------------------------------------------
COLOR_BG = (10, 10, 12)
COLOR_TEXT = (230, 230, 230)
COLOR_LABEL = (140, 140, 145)
COLOR_WARN = (220, 60, 50)
COLOR_OK = (60, 200, 120)
COLOR_STALE = (90, 90, 95)


def sensor_reader_thread(state: ClusterState, stop_event: threading.Event):
    """Hilo de fondo: lee tramas del puerto serie y alimenta ClusterState.
    Aisla completamente el I/O de puerto serie del hilo de render."""
    reader = ProtoReader(config.SENSOR_PORT, baudrate=config.SENSOR_BAUDRATE)
    try:
        for frame in reader.read_frames():
            if stop_event.is_set():
                break
            state.apply_frame(frame)
    finally:
        reader.close()


def format_value(sensor_value, unit: str, fmt: str = '{:.0f}') -> str:
    """Devuelve el valor formateado, o 'NO DATA' si esta obsoleto segun
    DOC-PH1-PROTO-001 Seccion 7 - nunca muestra el ultimo valor conocido
    sin avisar que ya no es confiable."""
    if sensor_value.is_stale():
        return "NO DATA"
    return fmt.format(sensor_value.value) + unit


def draw_gauge_text(surface, font, label, value_text, x, y, is_stale, align='left'):
    color = COLOR_STALE if is_stale else COLOR_TEXT
    label_surf = font_small.render(label, True, COLOR_LABEL)
    value_surf = font.render(value_text, True, color)
    if align == 'left':
        surface.blit(label_surf, (x, y))
        surface.blit(value_surf, (x, y + 22))
    else:
        surface.blit(label_surf, (x - label_surf.get_width(), y))
        surface.blit(value_surf, (x - value_surf.get_width(), y + 22))


def main():
    pygame.init()
    flags = pygame.FULLSCREEN if config.FULLSCREEN else 0
    screen = pygame.display.set_mode((config.SCREEN_WIDTH, config.SCREEN_HEIGHT), flags)
    pygame.display.set_caption("[Marca] Cluster")
    clock = pygame.time.Clock()

    global font_small
    font_small = pygame.font.SysFont('DejaVu Sans', 18)
    font_medium = pygame.font.SysFont('DejaVu Sans', 32, bold=True)
    font_large = pygame.font.SysFont('DejaVu Sans', 64, bold=True)

    state = ClusterState()
    stop_event = threading.Event()

    reader_thread = threading.Thread(
        target=sensor_reader_thread, args=(state, stop_event), daemon=True)
    reader_thread.start()

    heartbeat = HeartbeatLink(config.HEARTBEAT_PORT, own_node_id=NodeId.PI1,
                               baudrate=config.HEARTBEAT_BAUDRATE)
    heartbeat.start()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False  # salida para pruebas en banco; quitar en produccion

        screen.fill(COLOR_BG)

        # --- RPM (centro-izquierda, el dato mas prominente) ---
        rpm_text = format_value(state.rpm, '', '{:.0f}')
        draw_gauge_text(screen, font_large, "RPM", rpm_text, 60, 60, state.rpm.is_stale())

        # --- Velocidad ---
        speed_text = format_value(state.speed, ' km/h', '{:.0f}')
        draw_gauge_text(screen, font_large, "VELOCIDAD", speed_text, 420, 60, state.speed.is_stale())

        # --- Temperatura / presion de aceite ---
        temp_text = format_value(state.coolant_temp, ' C', '{:.0f}')
        draw_gauge_text(screen, font_medium, "TEMP. MOTOR", temp_text, 820, 60, state.coolant_temp.is_stale())
        oil_text = format_value(state.oil_pressure, ' PSI', '{:.0f}')
        draw_gauge_text(screen, font_medium, "PRESION ACEITE", oil_text, 820, 150, state.oil_pressure.is_stale())

        # --- Marcha ---
        gear_labels = {0: 'P', 1: 'R', 2: 'N', 3: 'D1', 4: 'D2', 5: 'D3',
                       6: 'D4', 7: 'D5', 8: 'D6'}
        if state.gear.is_stale():
            gear_text = "NO DATA"
        else:
            gear_text = gear_labels.get(int(state.gear.value), '?')
        draw_gauge_text(screen, font_large, "MARCHA", gear_text, 1080, 60, state.gear.is_stale())

        # --- Pitch / Roll ---
        pitch_text = format_value(state.pitch, ' deg', '{:.1f}')
        draw_gauge_text(screen, font_medium, "PITCH", pitch_text, 1280, 60, state.pitch.is_stale())
        roll_text = format_value(state.roll, ' deg', '{:.1f}')
        draw_gauge_text(screen, font_medium, "ROLL", roll_text, 1280, 150, state.roll.is_stale())

        # --- TPMS (4 esquinas) ---
        tpms_items = [
            ("FL", state.tpms_fl), ("FR", state.tpms_fr),
            ("RL", state.tpms_rl), ("RR", state.tpms_rr),
        ]
        for i, (label, sv) in enumerate(tpms_items):
            x = 1560 + (i % 2) * 170
            y = 60 + (i // 2) * 90
            if sv.is_stale():
                text = "NO DATA"
            else:
                psi, temp_c = sv.value
                text = f"{psi} PSI"
            draw_gauge_text(screen, font_small, f"TPMS {label}", text, x, y, sv.is_stale())

        # --- DTC (solo si hay codigo activo, no periodico) ---
        if not state.dtc.is_stale() and state.dtc.value != 0:
            dtc_surf = font_medium.render(f"DTC: 0x{int(state.dtc.value):04X}", True, COLOR_WARN)
            screen.blit(dtc_surf, (60, 400))

        # --- Indicador de estado de Pi 2 (heartbeat) - discreto, nunca
        # bloquea ni oculta el resto del cluster (Seccion 8) ---
        peer_color = COLOR_OK if heartbeat.is_peer_alive() else COLOR_WARN
        pygame.draw.circle(screen, peer_color, (config.SCREEN_WIDTH - 20, 20), 6)

        pygame.display.flip()
        clock.tick(config.TARGET_FPS)

    stop_event.set()
    heartbeat.stop()
    pygame.quit()
    sys.exit(0)


if __name__ == '__main__':
    main()
