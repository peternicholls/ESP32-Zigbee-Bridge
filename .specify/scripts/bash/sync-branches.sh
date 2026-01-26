#!/bin/bash
# sync-branches.sh - Sync all feature branches with main after a merge
#
# Usage: 
#   ./sync-branches.sh           # Rebase all feature branches onto main
#   ./sync-branches.sh --merge   # Merge main into all feature branches
#   ./sync-branches.sh --dry-run # Show what would be done without doing it

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh" 2>/dev/null || true

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

MODE="rebase"
DRY_RUN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --merge)
            MODE="merge"
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [--merge] [--dry-run]"
            echo ""
            echo "Sync all feature branches (###-*) with main branch."
            echo ""
            echo "Options:"
            echo "  --merge    Merge main into branches (creates merge commits)"
            echo "  --dry-run  Show what would be done without making changes"
            echo ""
            echo "Default behavior is to rebase branches onto main (cleaner history)."
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Ensure we're in a git repo
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

# Save current branch
CURRENT_BRANCH=$(git branch --show-current)

# Fetch latest
echo -e "${BLUE}Fetching latest from origin...${NC}"
if [ "$DRY_RUN" = false ]; then
    git fetch --all --prune
fi

# Find all feature branches (local and remote)
echo -e "${BLUE}Finding feature branches...${NC}"
FEATURE_BRANCHES=$(git branch --list '[0-9][0-9][0-9]-*' | sed 's/^[* ]*//')

if [ -z "$FEATURE_BRANCHES" ]; then
    echo -e "${YELLOW}No feature branches found (pattern: ###-*)${NC}"
    exit 0
fi

echo -e "${GREEN}Found feature branches:${NC}"
echo "$FEATURE_BRANCHES" | while read branch; do
    echo "  - $branch"
done
echo ""

# Check for uncommitted changes
if ! git diff-index --quiet HEAD -- 2>/dev/null; then
    echo -e "${RED}Error: You have uncommitted changes. Please commit or stash them first.${NC}"
    exit 1
fi

# Update main first
echo -e "${BLUE}Updating main branch...${NC}"
if [ "$DRY_RUN" = false ]; then
    git checkout main
    git pull --ff-only origin main 2>/dev/null || git pull origin main
fi

# Sync each feature branch
FAILED_BRANCHES=""
SUCCESS_COUNT=0

echo "$FEATURE_BRANCHES" | while read branch; do
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Processing: $branch${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}[DRY RUN] Would $MODE $branch onto/with main${NC}"
        continue
    fi
    
    git checkout "$branch"
    
    if [ "$MODE" = "rebase" ]; then
        echo -e "${YELLOW}Rebasing $branch onto main...${NC}"
        if git rebase main; then
            echo -e "${GREEN}✓ Successfully rebased $branch${NC}"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo -e "${RED}✗ Rebase failed for $branch - aborting rebase${NC}"
            git rebase --abort 2>/dev/null || true
            FAILED_BRANCHES="$FAILED_BRANCHES $branch"
        fi
    else
        echo -e "${YELLOW}Merging main into $branch...${NC}"
        if git merge main -m "Merge main into $branch"; then
            echo -e "${GREEN}✓ Successfully merged main into $branch${NC}"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo -e "${RED}✗ Merge failed for $branch - aborting merge${NC}"
            git merge --abort 2>/dev/null || true
            FAILED_BRANCHES="$FAILED_BRANCHES $branch"
        fi
    fi
done

# Return to original branch
echo ""
echo -e "${BLUE}Returning to $CURRENT_BRANCH...${NC}"
if [ "$DRY_RUN" = false ]; then
    git checkout "$CURRENT_BRANCH"
fi

# Summary
echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Summary${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if [ -n "$FAILED_BRANCHES" ]; then
    echo -e "${RED}Failed branches (resolve conflicts manually):${NC}"
    echo "$FAILED_BRANCHES" | tr ' ' '\n' | grep -v '^$' | while read branch; do
        echo -e "  ${RED}✗ $branch${NC}"
    done
    echo ""
    echo -e "${YELLOW}To resolve conflicts manually:${NC}"
    echo "  git checkout <branch>"
    echo "  git $MODE main"
    echo "  # resolve conflicts"
    echo "  git $MODE --continue"
fi

echo -e "${GREEN}Sync complete.${NC}"
