# gitlab_issue_reader.py
# This script demonstrates how to read issues from a GitLab project using the python-gitlab library.

import gitlab
from gitlab.exceptions import GitlabAuthenticationError, GitlabGetError

def read_gitlab_issues(gitlab_url: str, private_token: str, project_name: str):
    """
    Connects to GitLab and reads the titles and statuses of open issues 
    for a specified project.
    """
    print(f"Attempting to connect to GitLab at {gitlab_url}...")
    try:
        # Initialize connection using private token for authentication
        gl = gitlab.Gitlab(gitlab_url, private_token=private_token)
        
        # Get the specific project object
        project = gl.projects.get(project_name)
        print(f"Successfully connected to project: {project.name}")

    except GitlabAuthenticationError:
        print("\n[ERROR] Authentication failed. Please check your GitLab URL and private token.")
        return
    except GitlabGetError as e:
        print(f"\n[ERROR] Could not find the specified project '{project_name}'. Details: {e}")
        return

    # Fetch open issues (status is 'open' by default, but we specify it for clarity)
    try:
        issues = project.issue(Issue)
        print("\n--- Found Open Issues ---")
        
        # Limit to the first 10 for display purposes
        for i, issue in enumerate(issues[:10]):
            print(f"  {i+1}. Title: {issue.title}")
            print(f"     Status: {issue.state.capitalize()}")
            print(f"     URL: {issue.web_url}")
        
        if not issues:
             print("No open issues found in this project.")

    except Exception as e:
        print(f"\n[ERROR] An unexpected error occurred while fetching issues: {e}")


if __name__ == "__main__":
    # --- Configuration ---
    # IMPORTANT: Replace these placeholders with your actual credentials and project details.
    GITLAB_URL = "https://gitlab.com"  # e.g., "https://your-company.gitlab.com"
    PRIVATE_TOKEN = "YOUR_PRIVATE_ACCESS_TOKEN" # Token must have 'read_api' scope for Issues
    PROJECT_NAME = "your-group/your-repository-name"

    if PRIVATE_TOKEN == "YOUR_PRIVATE_ACCESS_TOKEN":
        print("*********************************************************************")
        print("[SETUP REQUIRED] Please update the script with your actual GITLAB_URL, ")
        print("PRIVATE_TOKEN, and PROJECT_NAME before running.")
        print("*********************************************************************")
    else:
        read_gitlab_issues(GITLAB_URL, PRIVATE_TOKEN, PROJECT_NAME)