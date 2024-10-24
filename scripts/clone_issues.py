import requests
import json

# GitHub API URLs
GITHUB_API_URL = "https://api.github.com"
ORIGINAL_REPO = "Avnu/gptp"
FORKED_REPO = "githubnext/workspace-blank"

# GitHub Personal Access Token
ACCESS_TOKEN = "your_personal_access_token"

# Headers for authentication
HEADERS = {
    "Authorization": f"token {ACCESS_TOKEN}",
    "Accept": "application/vnd.github.v3+json"
}

def fetch_issues(repo, page=1):
    url = f"{GITHUB_API_URL}/repos/{repo}/issues"
    params = {"page": page, "per_page": 100}
    response = requests.get(url, headers=HEADERS, params=params)
    response.raise_for_status()
    return response.json()

def create_issue(repo, issue):
    url = f"{GITHUB_API_URL}/repos/{repo}/issues"
    data = {
        "title": issue["title"],
        "body": issue["body"],
        "labels": [label["name"] for label in issue["labels"]]
    }
    response = requests.post(url, headers=HEADERS, data=json.dumps(data))
    response.raise_for_status()
    return response.json()

def clone_issues():
    page = 1
    while True:
        issues = fetch_issues(ORIGINAL_REPO, page)
        if not issues:
            break
        for issue in issues:
            create_issue(FORKED_REPO, issue)
        page += 1

if __name__ == "__main__":
    clone_issues()
