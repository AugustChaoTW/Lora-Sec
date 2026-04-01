#!/usr/bin/env python3
"""
Tamarin Verification Results Parser
Extracts lemma results, attack traces, and statistics from Tamarin logs
"""

import sys
import re
import json
from pathlib import Path
from typing import Dict, List, Tuple


class TamarinResultsParser:
    def __init__(self, log_file: str):
        self.log_file = Path(log_file)
        self.content = self.log_file.read_text()
        self.results = {}

    def parse(self) -> Dict:
        """Main parser method"""
        results = {
            "file": str(self.log_file),
            "summary": self._parse_summary(),
            "lemmas": self._parse_lemmas(),
            "timing": self._parse_timing(),
            "statistics": self._calculate_stats(),
        }
        return results

    def _parse_summary(self) -> Dict:
        """Extract summary section"""
        pattern = (
            r"summary of summaries:\s*analyzed:\s*(.+?)\s+processing time:\s*([\d.]+)s"
        )
        match = re.search(pattern, self.content)
        if match:
            return {
                "analyzed_file": match.group(1),
                "processing_time_sec": float(match.group(2)),
            }
        return {}

    def _parse_lemmas(self) -> List[Dict]:
        """Extract individual lemma results"""
        # Pattern: lemma_name (type): status (steps)
        pattern = r"(\S+)\s+\(([^)]+)\):\s+(verified|falsified|analysis incomplete)\s+(?:\((\d+)\s+steps\))?(?:\s*-\s*(.+?))?"

        lemmas = []
        for match in re.finditer(pattern, self.content):
            lemma = {
                "name": match.group(1),
                "type": match.group(2),
                "status": match.group(3),
                "steps": int(match.group(4)) if match.group(4) else None,
                "note": match.group(5) if match.group(5) else None,
            }
            lemmas.append(lemma)
        return lemmas

    def _parse_timing(self) -> Dict:
        """Extract timing information"""
        processing_match = re.search(r"processing time:\s*([\d.]+)s", self.content)
        if processing_match:
            return {"total_seconds": float(processing_match.group(1))}
        return {}

    def _calculate_stats(self) -> Dict:
        """Calculate statistics from parsed lemmas"""
        lemmas = self._parse_lemmas()

        if not lemmas:
            return {}

        verified = sum(1 for l in lemmas if l["status"] == "verified")
        falsified = sum(1 for l in lemmas if l["status"] == "falsified")
        incomplete = sum(1 for l in lemmas if l["status"] == "analysis incomplete")

        total_steps = sum(l["steps"] for l in lemmas if l["steps"])
        avg_steps = total_steps / len(lemmas) if lemmas else 0

        return {
            "total_lemmas": len(lemmas),
            "verified": verified,
            "falsified": falsified,
            "incomplete": incomplete,
            "success_rate": verified / len(lemmas) * 100 if lemmas else 0,
            "average_steps": avg_steps,
            "total_steps": total_steps,
        }

    def format_table(self) -> str:
        """Format results as ASCII table"""
        lemmas = self._parse_lemmas()
        if not lemmas:
            return "No lemmas found"

        # Header
        lines = [
            "Lemma Results",
            "=" * 80,
            f"{'Lemma Name':<40} {'Type':<15} {'Status':<15} {'Steps':>6}",
            "-" * 80,
        ]

        # Rows
        for lemma in lemmas:
            steps_str = str(lemma["steps"]) if lemma["steps"] else "-"
            status = lemma["status"]
            if status == "verified":
                status = "✓ " + status
            elif status == "falsified":
                status = "✗ " + status

            lines.append(
                f"{lemma['name']:<40} {lemma['type']:<15} {status:<15} {steps_str:>6}"
            )

        lines.append("=" * 80)

        # Statistics
        stats = self._calculate_stats()
        if stats:
            lines.append(f"Total lemmas: {stats['total_lemmas']}")
            lines.append(
                f"Verified: {stats['verified']}/{stats['total_lemmas']} ({stats['success_rate']:.1f}%)"
            )
            lines.append(f"Falsified: {stats['falsified']}")
            lines.append(f"Incomplete: {stats['incomplete']}")
            lines.append(
                f"Processing time: {self._parse_timing().get('total_seconds', 0):.2f}s"
            )

        return "\n".join(lines)

    def to_json(self) -> str:
        """Export results as JSON"""
        return json.dumps(self.parse(), indent=2)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 parse_results.py <log_file> [--json|--table]")
        print("  --json   Output as JSON (default)")
        print("  --table  Output as ASCII table")
        sys.exit(1)

    log_file = sys.argv[1]
    format_type = sys.argv[2] if len(sys.argv) > 2 else "--json"

    if not Path(log_file).exists():
        print(f"Error: File not found: {log_file}")
        sys.exit(1)

    parser = TamarinResultsParser(log_file)

    if format_type == "--table":
        print(parser.format_table())
    else:  # Default to JSON
        print(parser.to_json())


if __name__ == "__main__":
    main()
