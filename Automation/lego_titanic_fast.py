from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.chrome.service import Service
from webdriver_manager.chrome import ChromeDriverManager
import threading
import time

TITANIC_URL = "https://www.lego.com/en-us/product/titanic-10294"

def js_click(driver, el):
    driver.execute_script("arguments[0].click();", el)

def click_if_visible(driver, *texts, timeout=5):
    """Click the first visible button matching any text, within timeout."""
    try:
        def find_btn(d):
            for btn in d.find_elements(By.TAG_NAME, "button"):
                if btn.is_displayed():
                    for text in texts:
                        if text.lower() in btn.text.strip().lower():
                            return btn
            return False
        btn = WebDriverWait(driver, timeout).until(find_btn)
        print(f"  Clicking: '{btn.text.strip()}'")
        js_click(driver, btn)
        return True
    except:
        return False

def watch_for_continue(driver, stop_event):
    """Background thread: clicks 'Continue' popup the moment it appears."""
    while not stop_event.is_set():
        try:
            for btn in driver.find_elements(By.TAG_NAME, "button"):
                if btn.is_displayed() and btn.text.strip().lower() == "continue":
                    print("  [bg] Clicking 'Continue'")
                    js_click(driver, btn)
                    return
        except:
            pass
        time.sleep(0.3)

def run():
    options = Options()
    options.add_argument("--start-maximized")
    options.add_argument("--disable-blink-features=AutomationControlled")
    options.add_experimental_option("excludeSwitches", ["enable-automation"])
    options.add_experimental_option("useAutomationExtension", False)

    driver = webdriver.Chrome(service=Service(ChromeDriverManager().install()), options=options)
    driver.execute_script("Object.defineProperty(navigator, 'webdriver', {get: () => undefined})")

    try:
        # Go straight to product page — skip home page entirely
        print("Navigating directly to Titanic product page...")
        driver.get(TITANIC_URL)

        # Background thread catches "Continue" popup the instant it appears
        stop_event = threading.Event()
        t = threading.Thread(target=watch_for_continue, args=(driver, stop_event), daemon=True)
        t.start()

        # Reject cookies as soon as banner appears
        print("Waiting for cookie banner...")
        click_if_visible(driver, "reject all", "deny", "decline", timeout=10)

        # Wait for Continue thread to finish
        t.join(timeout=10)
        stop_event.set()

        # Add to Bag
        print("Adding to bag...")
        click_if_visible(driver, "add to bag", timeout=10)

        # View cart
        print("Viewing cart...")
        click_if_visible(driver, "view my bag", "view bag", timeout=8)

        print("\nDone! Viewing Titanic in cart. Closing in 15 seconds...")
        time.sleep(15)

    except Exception as e:
        print(f"Error: {e}")
        time.sleep(5)
    finally:
        driver.quit()

if __name__ == "__main__":
    run()
