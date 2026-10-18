"""Anonymous public mirror check; never executes code or deletes copies."""
import json
import sys
import urllib.error
import urllib.request

REPO = "g0v/liquid_feedback_core"
PIN = "39ee1b21318871132f401da5444a11ebe9e6932c"


def get(path):
    request = urllib.request.Request("https://api.github.com/repos/" + REPO + path,
                                    headers={"User-Agent": "group-ricochet-provenance-check",
                                             "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def main():
    try:
        repo = get("")
        if repo.get("private") is not False or repo.get("full_name") != REPO:
            print("ATTENTION: public identity/visibility changed; retain existing references/copies.")
            return 1
        get("/commits/" + PIN)
        licence = get("/contents/LICENSE?ref=" + PIN)
        if licence.get("type") != "file":
            print("ATTENTION: pinned licence file not available; review before reuse.")
            return 1
        head = get("/commits/" + repo["default_branch"])["sha"]
        print(json.dumps(dict(repository=REPO, publicly_accessible=True, pinned_revision=PIN,
                              current_revision=head, archived=repo["archived"],
                              update_available=head != PIN, integrated=False)))
        return 0
    except urllib.error.HTTPError as error:
        code = error.code
        error.close()
        if code == 404:
            print("ATTENTION: reference no longer publicly accessible (404); retain references/copies. Not proof of deletion.")
            return 1
        print(f"INCONCLUSIVE: GitHub HTTP {code}; do not infer removal.")
        return 2
    except (OSError, ValueError, KeyError):
        print("INCONCLUSIVE: network/API response problem; do not infer removal.")
        return 2


if __name__ == "__main__":
    sys.exit(main())
