import tkinter as tk
from tkinter import ttk
import threading
import time
import re
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.chrome.service import Service
from webdriver_manager.chrome import ChromeDriverManager

def js_click(driver, el):
    driver.execute_script("arguments[0].click();", el)

def make_driver():
    options = Options()
    options.add_argument("--start-maximized")
    options.add_argument("--disable-blink-features=AutomationControlled")
    options.add_argument("--log-level=3")
    options.add_experimental_option("excludeSwitches", ["enable-automation"])
    options.add_experimental_option("useAutomationExtension", False)
    driver = webdriver.Chrome(service=Service(ChromeDriverManager().install()), options=options)
    driver.execute_script("Object.defineProperty(navigator, 'webdriver', {get: () => undefined})")
    return driver

def dismiss_popups(driver):
    """Click Continue and cookie rejection buttons if present."""
    for btn in driver.find_elements(By.TAG_NAME, "button"):
        try:
            if btn.is_displayed() and btn.text.strip().lower() == "continue":
                js_click(driver, btn)
                time.sleep(0.5)
                break
        except:
            pass
    for btn in driver.find_elements(By.TAG_NAME, "button"):
        try:
            if btn.is_displayed() and any(x in btn.text.lower() for x in ["reject", "deny", "decline"]):
                js_click(driver, btn)
                time.sleep(0.5)
                break
        except:
            pass

def scrape_set(driver, query, first):
    search_url = f"https://www.lego.com/en-us/search?q={query.replace(' ', '+')}"
    driver.get(search_url)

    # On first search, wait longer for the Continue popup to appear then dismiss
    if first:
        time.sleep(6)
        dismiss_popups(driver)
        time.sleep(2)
    else:
        time.sleep(3)

    # Wait for product cards to load
    try:
        WebDriverWait(driver, 10).until(
            lambda d: len(d.find_elements(By.CSS_SELECTOR, "[data-test='product-leaf']")) > 0
        )
    except:
        raise RuntimeError(f"No results found for '{query}'")

    leaves = driver.find_elements(By.CSS_SELECTOR, "[data-test='product-leaf']")
    if not leaves:
        raise RuntimeError(f"No results found for '{query}'")

    leaf = leaves[0]

    # Name
    name = query
    try:
        el = leaf.find_element(By.CSS_SELECTOR, "[data-test='product-leaf-title']")
        name = el.text.strip()
    except:
        for line in leaf.text.splitlines():
            line = line.strip()
            if line and re.search(r"[A-Za-z]{3,}", line) and "$" not in line and "Bag" not in line:
                name = line
                break

    # Price
    price = None
    try:
        el = leaf.find_element(By.CSS_SELECTOR, "[data-test='price-container']")
        m = re.search(r"\$([\d,]+\.?\d*)", el.text)
        if m:
            price = float(m.group(1).replace(",", ""))
    except:
        pass
    if price is None:
        m = re.search(r"\$([\d,]+\.?\d*)", leaf.text)
        if m:
            price = float(m.group(1).replace(",", ""))

    # Piece count
    pieces = None
    try:
        el = leaf.find_element(By.CSS_SELECTOR, "[data-test='product-leaf-piece-count-label']")
        m = re.search(r"([\d,]+)", el.text)
        if m:
            pieces = int(m.group(1).replace(",", ""))
    except:
        pass
    if pieces is None:
        nums = re.findall(r"^(\d{2,5})$", leaf.text, re.MULTILINE)
        if nums:
            pieces = int(nums[0])

    if price is None:
        raise RuntimeError(f"Could not find price for '{query}'")
    if pieces is None:
        raise RuntimeError(f"Could not find piece count for '{query}'")

    return name, price, pieces, price / pieces

def run_search(sets, status_var, btn):
    print("\n" + "=" * 57)
    print(f"{'SET':<32} {'PRICE':>8} {'PIECES':>7} {'¢/PIECE':>8}")
    print("=" * 57)

    driver = make_driver()
    try:
        for i, query in enumerate(sets):
            status_var.set(f"Searching: {query}...")
            try:
                name, price, pieces, ppp = scrape_set(driver, query, first=(i == 0))
                print(f"{name[:32]:<32} ${price:>7.2f} {pieces:>7,} {ppp*100:>7.2f}¢")
            except RuntimeError as e:
                print(f"  ERROR [{query}]: {e}")
    finally:
        driver.quit()

    print("=" * 57)
    status_var.set("Done.")
    btn.config(state=tk.NORMAL)

def on_search(text_widget, status_var, btn):
    raw = text_widget.get("1.0", tk.END)
    sets = [s.strip() for s in re.split(r"[,\n]+", raw) if s.strip()]
    if not sets:
        status_var.set("Enter at least one set name or number.")
        return
    btn.config(state=tk.DISABLED)
    status_var.set("Starting...")
    threading.Thread(target=run_search, args=(sets, status_var, btn), daemon=True).start()

def main():
    root = tk.Tk()
    root.title("LEGO Price Checker")
    root.resizable(False, False)

    frame = ttk.Frame(root, padding=16)
    frame.grid()

    ttk.Label(frame, text="Enter LEGO set names or numbers\n(comma or newline separated):").grid(
        row=0, column=0, columnspan=2, sticky="w", pady=(0, 6))

    text = tk.Text(frame, width=44, height=8, font=("Segoe UI", 10))
    text.grid(row=1, column=0, columnspan=2, pady=(0, 10))
    text.insert("1.0", "Titanic 10294\nMillennium Falcon 75192\nEiffel Tower 10307")

    status_var = tk.StringVar(value="Ready")
    ttk.Label(frame, textvariable=status_var, foreground="gray").grid(
        row=2, column=0, sticky="w")

    btn = ttk.Button(frame, text="Search")
    btn.config(command=lambda: on_search(text, status_var, btn))
    btn.grid(row=2, column=1, sticky="e")

    root.mainloop()

if __name__ == "__main__":
    main()
