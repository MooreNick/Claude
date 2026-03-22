from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.chrome.service import Service
from webdriver_manager.chrome import ChromeDriverManager
import time
import os

LEGO_HOME = "https://www.lego.com/en-us"
TITANIC_URL = "https://www.lego.com/en-us/product/titanic-10294"
SCREENSHOT_DIR = os.path.dirname(__file__)

def js_click(driver, element):
    driver.execute_script("arguments[0].click();", element)

def wait_and_click_button(driver, *texts, timeout=20):
    """Wait for a visible button matching any text fragment, then click it."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        buttons = driver.find_elements(By.TAG_NAME, "button")
        for btn in buttons:
            if not btn.is_displayed():
                continue
            label = btn.text.strip().lower()
            for text in texts:
                if text.lower() in label:
                    print(f"  Found button: '{btn.text.strip()}' — clicking")
                    js_click(driver, btn)
                    time.sleep(1)
                    return True
        time.sleep(0.5)
    print(f"  Timed out waiting for button: {texts}")
    return False

def add_titanic_to_cart():
    options = Options()
    options.add_argument("--start-maximized")
    options.add_argument("--disable-blink-features=AutomationControlled")
    options.add_experimental_option("excludeSwitches", ["enable-automation"])
    options.add_experimental_option("useAutomationExtension", False)

    driver = webdriver.Chrome(service=Service(ChromeDriverManager().install()), options=options)
    driver.execute_script("Object.defineProperty(navigator, 'webdriver', {get: () => undefined})")

    try:
        # Step 1: Open lego.com
        print("Step 1: Opening lego.com...")
        driver.get(LEGO_HOME)

        # Step 2: Wait for and click the "Continue" button
        print("Step 2: Waiting for 'Continue' button...")
        wait_and_click_button(driver, "continue", timeout=20)
        driver.save_screenshot(os.path.join(SCREENSHOT_DIR, "lego_after_continue.png"))

        # Step 3: Handle cookie banner — decline
        print("Step 3: Handling cookie banner...")
        wait_and_click_button(driver, "deny", "decline", "reject", "refuse", "no thanks", timeout=10)
        driver.save_screenshot(os.path.join(SCREENSHOT_DIR, "lego_after_cookies.png"))

        # Step 4: Navigate to Titanic product page and add to cart
        print("Step 4: Navigating to LEGO Titanic set...")
        driver.get(TITANIC_URL)
        time.sleep(3)

        print("  Clicking 'Add to Bag'...")
        wait_and_click_button(driver, "add to bag", timeout=15)
        driver.save_screenshot(os.path.join(SCREENSHOT_DIR, "lego_after_add.png"))
        print("  Titanic set added to bag!")

        # Step 5: Click "View My Bag"
        print("Step 5: Viewing cart...")
        wait_and_click_button(driver, "view my bag", "view bag", "view cart", timeout=10)
        driver.save_screenshot(os.path.join(SCREENSHOT_DIR, "lego_cart.png"))
        print("  Cart opened!")

        print("\nDone! Browser closing in 15 seconds...")
        time.sleep(15)

    except Exception as e:
        print(f"Error: {e}")
        driver.save_screenshot(os.path.join(SCREENSHOT_DIR, "lego_error.png"))
        time.sleep(10)
    finally:
        driver.quit()

if __name__ == "__main__":
    add_titanic_to_cart()
