import pygame
import cv2
import mediapipe as mp
import math
import os
import random
from PIL import Image, ImageSequence
import socket

# --- ค่าคงที่ (Constants) ---
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
TARGET_FOLDER = 'target'
TARGET_SIZE = (80, 80)
BULLET_SIZE = (30, 30)
EFFECT_SIZE = (150, 150)
FINGER_IMAGE_SIZE = (50, 50)
FINGER_IMAGE_OFFSET_Y = 20  # เลื่อนภาพปลายนิ้วชี้ลงมา 20 pixel จากจุดกึ่งกลางปลายนิ้ว
TARGET_COUNT = 5
TARGET_SPEED_RANGE = (2, 5)
BULLET_SPEED = 15
BULLET_ROTATION_SPEED = 10
GAME_DURATION_SEC = 60
OUTLINE_COLOR = (0, 0, 0)

# --- ฟอนต์สำหรับคะแนนลอย ---
POPUP_SCORE_FONT = None
POPUP_SCORE_COLOR = (255, 255, 0)  # สีคะแนนตอนยิงโดนเป้าหมาย
POPUP_SCORE_LIFESPAN = 30
POPUP_SCORE_SPEED = 2

# ฟอนต์สำหรับ UI อื่นๆ
TIMER_FONT = None
GAME_OVER_FONT = None
FINAL_SCORE_FONT = None

# --- ฟังก์ชันสำหรับ Resize ภาพโดยรักษาสัดส่วน ---
def scale_image_aspect_ratio(image, max_size):
    img_width, img_height = image.get_size()
    max_width, max_height = max_size
    img_ratio = img_width / img_height
    max_ratio = max_width / max_height

    if img_ratio > max_ratio:
        new_width = max_width
        new_height = int(new_width / img_ratio)
    else:
        new_height = max_height
        new_width = int(new_height * img_ratio)

    return pygame.transform.smoothscale(image, (new_width, new_height))


# --- ฟังก์ชันสำหรับวาดขอบอักษร ---
def draw_text_with_outline(screen, text, font, main_color, outline_color, center_pos, outline_width=3):
    """
    วาดข้อความที่มีขอบ (Outline) โดยการวาด 8 ทิศทาง
    """
    text_surface_main = font.render(text, True, main_color)
    text_surface_outline = font.render(text, True, outline_color)

    positions = []
    for dx in range(-outline_width, outline_width + 1, outline_width):
        for dy in range(-outline_width, outline_width + 1, outline_width):
            if dx == 0 and dy == 0:
                continue
            pos = (center_pos[0] + dx, center_pos[1] + dy)
            positions.append(text_surface_outline.get_rect(center=pos))

    screen.blits(list(zip([text_surface_outline] * len(positions), positions)))
    screen.blit(text_surface_main, text_surface_main.get_rect(center=center_pos))


# --- 1. คลาสสำหรับเป้าหมาย (Target Class) ---
class Target(pygame.sprite.Sprite):
    def __init__(self, image, score):
        super().__init__()
        self.original_image = scale_image_aspect_ratio(image, TARGET_SIZE)
        self.image = self.original_image
        self.score = score
        self.rect = self.image.get_rect()
        self.reset()

    def reset(self):
        self.rect.x = random.randint(0, SCREEN_WIDTH - self.rect.width)
        self.rect.y = random.randint(-SCREEN_HEIGHT, -self.rect.height)
        self.speed_y = random.randint(*TARGET_SPEED_RANGE)

    def update(self):
        self.rect.y += self.speed_y
        if self.rect.top > SCREEN_HEIGHT:
            self.kill()

    def draw(self, screen):
        screen.blit(self.image, self.rect)


# --- 2. คลาสสำหรับกระสุน (Bullet Class) ---
class Bullet(pygame.sprite.Sprite):
    def __init__(self, x, y, angle, image):
        super().__init__()
        self.original_image = scale_image_aspect_ratio(image, BULLET_SIZE)
        self.image = self.original_image
        self.rect = self.image.get_rect(center=(x, y))
        self.x = float(x)
        self.y = float(y)
        self.angle_rad = math.radians(angle)
        self.vel_x = math.cos(self.angle_rad) * BULLET_SPEED
        self.vel_y = -math.sin(self.angle_rad) * BULLET_SPEED
        self.rotation_angle = 0

    def update(self):
        self.x += self.vel_x
        self.y += self.vel_y
        self.rotation_angle = (self.rotation_angle + BULLET_ROTATION_SPEED) % 360
        self.image = pygame.transform.rotate(self.original_image, self.rotation_angle)
        self.rect = self.image.get_rect(center=(int(self.x), int(self.y)))
        if not pygame.Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT).colliderect(self.rect):
            self.kill()


# --- 3. คลาสสำหรับเอฟเฟกต์ GIF (SpecialEffect Class) ---
class SpecialEffect(pygame.sprite.Sprite):
    def __init__(self, center_pos, gif_path, max_size):
        super().__init__()
        self.frames = []
        self.max_size = max_size
        self.load_gif_pil(gif_path)
        if not self.frames:
            print(f"Error: Could not load GIF: {gif_path}")
            self.kill()
            return
        self.frame_index = 0
        self.image = self.frames[self.frame_index]
        self.rect = self.image.get_rect(center=center_pos)
        self.last_update = pygame.time.get_ticks()
        self.frame_rate = 50

    def load_gif_pil(self, gif_path):
        try:
            with Image.open(gif_path) as pil_gif:
                for frame in ImageSequence.Iterator(pil_gif):
                    frame_rgba = frame.convert('RGBA')
                    data = frame_rgba.tobytes()
                    size = frame_rgba.size
                    mode = frame_rgba.mode
                    surface_orig = pygame.image.fromstring(data, size, mode)
                    surface_scaled = scale_image_aspect_ratio(surface_orig, self.max_size)
                    surface_final = surface_scaled.convert_alpha()
                    self.frames.append(surface_final)
        except Exception as e:
            print(f"Error loading GIF with PIL: {e}")
            self.frames = []

    def update(self):
        now = pygame.time.get_ticks()
        if now - self.last_update > self.frame_rate:
            self.last_update = now
            self.frame_index += 1
            if self.frame_index == len(self.frames):
                self.kill()
            else:
                center = self.rect.center
                self.image = self.frames[self.frame_index]
                self.rect = self.image.get_rect(center=center)


# --- 4. คลาสสำหรับคะแนนลอย (ScorePopup Class) ---
class ScorePopup(pygame.sprite.Sprite):
    def __init__(self, center_pos, score_value):
        super().__init__()
        self.text = f"{score_value:+}"
        self.image = POPUP_SCORE_FONT.render(self.text, True, POPUP_SCORE_COLOR)
        self.rect = self.image.get_rect(center=center_pos)
        self.lifespan = POPUP_SCORE_LIFESPAN

    def update(self):
        self.rect.y -= POPUP_SCORE_SPEED
        self.lifespan -= 1
        if self.lifespan <= 0:
            self.kill()


# --- 5. ฟังก์ชันสำหรับโหลดทรัพยากร (แยกพื้นหลังออก) ---
def load_background_image():
    """ โหลดและปรับขนาดภาพพื้นหลัง """
    try:
        background_image = pygame.image.load('background.jpg').convert()
        background_image = pygame.transform.scale(background_image, (SCREEN_WIDTH, SCREEN_HEIGHT))
        return background_image
    except FileNotFoundError:
        print("Warning: 'background.jpg' not found. Game will use camera feed as background.")
        return None

def load_other_assets():
    """ โหลดทรัพยากรอื่นๆ ทั้งหมด (ยกเว้นพื้นหลัง) """
    target_data = []
    try:
        for filename in os.listdir(TARGET_FOLDER):
            if filename.endswith('.png'):
                try:
                    score = int(filename.split('_')[0])
                    img_path = os.path.join(TARGET_FOLDER, filename)
                    image = pygame.image.load(img_path).convert_alpha()
                    target_data.append((image, score))
                except (ValueError, IndexError):
                    print(f"Warning: Could not parse score from filename: {filename}")
    except FileNotFoundError:
        print(f"Error: Target folder '{TARGET_FOLDER}' not found.")
        return None, None, None
    if not target_data:
        print(f"Error: No target images found in '{TARGET_FOLDER}'.")
        return None, None, None

    try:
        bullet_image = pygame.image.load('bullet2.png').convert_alpha()  # ภาพกระสุน
    except FileNotFoundError:
        print("Error: 'bullet2.png' not found.")
        return None, None, None

    try:
        original_finger_image = pygame.image.load('GodGundam.png').convert_alpha()  # ภาพฐานยิง(เคลื่อนที่ตามนิ้วชี้)
        finger_image = scale_image_aspect_ratio(original_finger_image, FINGER_IMAGE_SIZE)
    except FileNotFoundError:
        print("Error: 'GodGundam.png' not found. Finger image will not be displayed.")
        finger_image = None

    return target_data, bullet_image, finger_image

# --- 6. ฟังก์ชันหลัก (Main Game Function) ---
def main():
    # --- 6.1. Initializations ---
    pygame.init()
    pygame.font.init()

    global POPUP_SCORE_FONT, TIMER_FONT, GAME_OVER_FONT, FINAL_SCORE_FONT
    try:
        POPUP_SCORE_FONT = pygame.font.Font(None, 48)
        TIMER_FONT = pygame.font.Font(None, 50)
        GAME_OVER_FONT = pygame.font.Font(None, 100)
        FINAL_SCORE_FONT = pygame.font.Font(None, 72)
    except Exception as e:
        print(f"Could not load default font: {e}. Using SysFont.")
        POPUP_SCORE_FONT = pygame.font.SysFont('Arial', 30)
        TIMER_FONT = pygame.font.SysFont('Arial', 32)
        GAME_OVER_FONT = pygame.font.SysFont('Arial', 64)
        FINAL_SCORE_FONT = pygame.font.SysFont('Arial', 48)

    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("Finger Shooting Game (Q=Quit, R=Restart)")

    # โหลด-วาด-แสดงผล พื้นหลังทันที
    background_image = load_background_image()
    if background_image:
        screen.blit(background_image, (0, 0))
        pygame.display.flip()

    clock = pygame.time.Clock()
    score_font = pygame.font.Font(None, 60)
    score = 0

    # โหลดทรัพยากรที่เหลือ (ขณะที่พื้นหลังแสดงอยู่)
    target_data, bullet_image, finger_image = load_other_assets()
    if target_data is None or bullet_image is None:
        return

    # โหลดกล้อง (ขณะที่พื้นหลังแสดงอยู่)
    cap = cv2.VideoCapture(0, cv2.CAP_DSHOW)
    if background_image is None and not cap.isOpened():
        print("Error: Cannot open camera and no background.jpg found. Exiting.")
        return
    elif background_image is None:
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, SCREEN_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, SCREEN_HEIGHT)

    # โหลด MediaPipe (ขณะที่พื้นหลังแสดงอยู่)
    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(
        max_num_hands=2,
        min_detection_confidence=0.7,
        min_tracking_confidence=0.5
    )

    all_sprites = pygame.sprite.Group()
    targets = pygame.sprite.Group()
    bullets = pygame.sprite.Group()
    effects = pygame.sprite.Group()
    popups = pygame.sprite.Group()

    game_state = "PLAYING"
    start_time = pygame.time.get_ticks()

    last_shot_time = {"Left": 0, "Right": 0}
    shoot_delay = 500  # หน่วงเวลา 500 มิลลิวินาที (ครึ่งวินาที ต่อ 1 นัด)

    # --- 6.2. Game Loop ---
    running = True
    # Setup UDP sender to send pointer state to C++ app
    udp_addr = ('127.0.0.1', 8765)
    try:
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_sock.setblocking(False)
    except Exception as e:
        print(f"Warning: UDP socket could not be created: {e}")
        udp_sock = None
    while running:

        # --- 6.2.1. Handle Events ---
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_q:
                    running = False

                if event.key == pygame.K_r:
                    score = 0
                    all_sprites.empty()
                    bullets.empty()
                    targets.empty()
                    effects.empty()
                    popups.empty()
                    start_time = pygame.time.get_ticks()
                    game_state = "PLAYING"

        # --- 6.2.2. วาดพื้นหลัง ---
        if background_image:
            screen.blit(background_image, (0, 0))
            mp_frame_source = cap.read()[1] if cap.isOpened() else None
            if mp_frame_source is not None:
                mp_frame_source = cv2.flip(mp_frame_source, 1)
            else:
                mp_frame_source = pygame.surfarray.array3d(screen).swapaxes(0, 1)
                mp_frame_source = cv2.cvtColor(mp_frame_source, cv2.COLOR_RGB2BGR)

        else:
            success, frame_from_cam = cap.read()
            if not success:
                continue
            frame_from_cam = cv2.flip(frame_from_cam, 1)
            frame_rgb_for_pygame = cv2.cvtColor(frame_from_cam, cv2.COLOR_BGR2RGB)
            frame_surface = pygame.surfarray.make_surface(frame_rgb_for_pygame.swapaxes(0, 1))
            screen.blit(frame_surface, (0, 0))
            mp_frame_source = frame_from_cam

        # --- State Machine ---
        if game_state == "PLAYING":
            # --- ตรรกะการเล่นเกม ---
            current_ticks = pygame.time.get_ticks()
            elapsed_sec = (current_ticks - start_time) // 1000
            remaining_time_sec = GAME_DURATION_SEC - elapsed_sec

            if remaining_time_sec <= 0:
                remaining_time_sec = 0
                game_state = "GAME_OVER"
                all_sprites.empty()
                bullets.empty()
                targets.empty()
                effects.empty()
                popups.empty()
                continue

            # CV Hand Tracking
            if mp_frame_source is not None:
                rgb_frame = cv2.cvtColor(mp_frame_source, cv2.COLOR_BGR2RGB)
                results = hands.process(rgb_frame)

                # Prepare per-frame pointer defaults
                left_x = 0.0; left_y = 0.0; left_pressed = 0
                right_x = 0.0; right_y = 0.0; right_pressed = 0

                # Shooting
                # เช็กทั้ง landmarks (ตำแหน่งข้อต่อ) และ handedness (ซ้าย/ขวา)
                if results.multi_hand_landmarks and results.multi_handedness:
                    for hand_landmarks, handedness in zip(results.multi_hand_landmarks, results.multi_handedness):
                        # ดึงป้ายกำกับมือว่าเป็น "Left" หรือ "Right"
                        hand_label = handedness.classification[0].label

                        idx_tip = hand_landmarks.landmark[mp_hands.HandLandmark.INDEX_FINGER_TIP]
                        idx_mcp = hand_landmarks.landmark[mp_hands.HandLandmark.INDEX_FINGER_MCP]
                        mid_tip = hand_landmarks.landmark[mp_hands.HandLandmark.MIDDLE_FINGER_TIP]

                        tip_x = int(idx_tip.x * SCREEN_WIDTH)
                        tip_y = int(idx_tip.y * SCREEN_HEIGHT)
                        norm_x = idx_tip.x
                        norm_y = idx_tip.y
                        mcp_x = int(idx_mcp.x * SCREEN_WIDTH)
                        mcp_y = int(idx_mcp.y * SCREEN_HEIGHT)

                        # คำนวณมุมสำหรับ Finger Image
                        angle_from_mcp_to_tip = math.degrees(math.atan2(tip_y - mcp_y, tip_x - mcp_x))
                        angle_for_image = -(angle_from_mcp_to_tip + 90)

                        # สำหรับกระสุน ให้ยิงไปตามทิศทางนิ้ว
                        angle_for_bullet = math.degrees(math.atan2(-(tip_y - mcp_y), (tip_x - mcp_x)))

                        current_time = pygame.time.get_ticks()
                        is_shooting = (idx_tip.y < mid_tip.y - 0.04)

                        # Assign to left/right pointers based on MediaPipe handedness
                        if hand_label == 'Left':
                            left_x = norm_x
                            left_y = norm_y
                            left_pressed = 1 if is_shooting else 0
                        elif hand_label == 'Right':
                            right_x = norm_x
                            right_y = norm_y
                            right_pressed = 1 if is_shooting else 0

                        # เช็กเงื่อนไข: ทำท่ายิง + เวลาปัจจุบันห่างจากเวลายิงล่าสุดของ "มือข้างนั้น" เกิน delay แล้ว
                        if is_shooting and (current_time - last_shot_time[hand_label] > shoot_delay):
                            bullet = Bullet(tip_x, tip_y, angle_for_bullet, bullet_image)
                            all_sprites.add(bullet)
                            bullets.add(bullet)
                            # อัปเดตเวลาล่าสุดเฉพาะของมือข้างที่เพิ่งยิงออกไป
                            last_shot_time[hand_label] = current_time

                        #  ใส่ภาพที่ปลายนิ้วชี้และหมุน พร้อมขยับตำแหน่ง
                        if finger_image:
                            rotated_finger_image = pygame.transform.rotate(finger_image, angle_for_image)
                            # คำนวณตำแหน่งใหม่ โดยใช้ vector จาก mcp ไป tip (ทิศทางนิ้ว)
                            # แล้วขยับตามทิศทางตรงกันข้ามเล็กน้อย

                            # แปลง angle_from_mcp_to_tip เป็น radian เพื่อคำนวณเวกเตอร์
                            angle_rad_for_offset = math.radians(angle_from_mcp_to_tip)

                            # คำนวณ delta_x, delta_y สำหรับการเลื่อน
                            # การเลื่อน "ลงมา" จากปลาย (ไปในทิศทางตรงกันข้ามกับที่นิ้วชี้)
                            # ดังนั้นใช้ cos และ sin ของมุมที่เพิ่ม 180 องศา (หรือ + math.pi)
                            offset_x = math.cos(angle_rad_for_offset + math.pi) * FINGER_IMAGE_OFFSET_Y
                            offset_y = math.sin(angle_rad_for_offset + math.pi) * FINGER_IMAGE_OFFSET_Y

                            # ใช้ tip_x, tip_y เป็นจุดอ้างอิง แล้วเพิ่ม offset
                            draw_x = tip_x + offset_x
                            draw_y = tip_y + offset_y

                            finger_rect = rotated_finger_image.get_rect(center=(int(draw_x), int(draw_y)))
                            screen.blit(rotated_finger_image, finger_rect)

            # Update Sprites
            all_sprites.update()

                # Send UDP packet with pointer state: lx,ly,lp,rx,ry,rp
                if udp_sock is not None:
                    msg = f"{left_x:.4f},{left_y:.4f},{int(left_pressed)},{right_x:.4f},{right_y:.4f},{int(right_pressed)}"
                    try:
                        udp_sock.sendto(msg.encode('ascii'), udp_addr)
                    except BlockingIOError:
                        pass
                    except Exception:
                        pass

            # เติมเป้าหมาย
            while len(targets) < TARGET_COUNT:
                img, s = random.choice(target_data)
                new_target = Target(img, s)
                all_sprites.add(new_target)
                targets.add(new_target)

            # Collision
            hits = pygame.sprite.groupcollide(bullets, targets, True, True)
            for bullet, hit_targets_list in hits.items():
                for target in hit_targets_list:
                    score += target.score
                    popup = ScorePopup(target.rect.center, target.score)
                    all_sprites.add(popup)
                    popups.add(popup)
                    if target.score == 10:
                        effect = SpecialEffect(target.rect.center, 'nemo.gif', EFFECT_SIZE)
                        all_sprites.add(effect)
                        effects.add(effect)

            # Drawing (Sprites)
            targets.draw(screen)
            bullets.draw(screen)
            effects.draw(screen)
            popups.draw(screen)

            # # วาดคะแนนรวม
            # score_text = score_font.render(f'SCORE: {score}', True, (255, 255, 0))
            # screen.blit(score_text, (10, 10))
            #
            # # วาดตัวจับเวลา
            # timer_text = TIMER_FONT.render(f"Time: {remaining_time_sec}", True, (255, 255, 255))
            # screen.blit(timer_text, (SCREEN_WIDTH - timer_text.get_width() - 10, 10))

            # 1. กำหนดสีหลักและคำนวณสีขอบ (255 - ค่าสีหลัก)
            score_color = (255, 255, 0)  # สีเหลือง สีคะแนน
            score_outline = (255 - score_color[0], 255 - score_color[1],
                             255 - score_color[2])  # จะได้ (0, 0, 255) สีน้ำเงิน

            time_color = (255, 255, 255)  # สีขาว
            time_outline = (255 - time_color[0], 255 - time_color[1], 255 - time_color[2])  # จะได้ (0, 0, 0) สีดำ

            # 2. เตรียมข้อความ
            score_str = f'SCORE: {score}'
            time_str = f"Time: {remaining_time_sec}"

            # 3. คำนวณหาจุดกึ่งกลาง (Center) ของข้อความเพื่อให้สอดคล้องกับฟังก์ชัน draw_text_with_outline
            score_w, score_h = score_font.size(score_str)
            time_w, time_h = TIMER_FONT.size(time_str)

            # ให้คะแนนอยู่มุมซ้ายบน (ห่างขอบ 10)
            score_center = (10 + (score_w // 2), 10 + (score_h // 2))

            # ให้เวลาอยู่มุมขวาบน (ห่างขอบ 10)
            time_center = (SCREEN_WIDTH - 10 - (time_w // 2), 10 + (time_h // 2))

            # 4. เรียกใช้ฟังก์ชันวาดขอบ
            draw_text_with_outline(screen, score_str, score_font, score_color, score_outline, score_center,
                                   outline_width=3)
            draw_text_with_outline(screen, time_str, TIMER_FONT, time_color, time_outline, time_center, outline_width=3)


        elif game_state == "GAME_OVER":
            center_x = SCREEN_WIDTH // 2

            draw_text_with_outline(
                screen, "TIME'S UP!", GAME_OVER_FONT,
                (255, 0, 0), OUTLINE_COLOR,
                (center_x, SCREEN_HEIGHT // 2 - 100),
                outline_width=3
            )

            draw_text_with_outline(
                screen, f"Final Score: {score}", FINAL_SCORE_FONT,
                (255, 255, 0), OUTLINE_COLOR,
                (center_x, SCREEN_HEIGHT // 2),
                outline_width=3
            )

            draw_text_with_outline(
                screen, "Press 'R' to Restart or 'Q' to Quit", TIMER_FONT,
                (255, 255, 255), OUTLINE_COLOR,
                (center_x, SCREEN_HEIGHT // 2 + 100),
                outline_width=2
            )

        # --- จบ State Machine ---

        pygame.display.flip()
        clock.tick(30)

        # --- 6.3. Cleanup ---
    if cap.isOpened():
        cap.release()
    cv2.destroyAllWindows()
    pygame.quit()


# --- 7. Run the game ---
if __name__ == "__main__":
    main()