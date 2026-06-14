#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "libavoid/geometry.h"
#include "libdialect/commontypes.h"
#include "libdialect/graphs.h"
#include "libdialect/hola.h"
#include "libdialect/io.h"
#include "libdialect/opts.h"

using namespace dialect;
using Avoid::Point;

namespace {

struct Segment {
    Point a;
    Point b;
};

constexpr double EPS = 1e-6;

double dist(const Point &p, const Point &q) {
    const double dx = p.x - q.x;
    const double dy = p.y - q.y;
    return std::hypot(dx, dy);
}

double dot(const Point &u, const Point &v) {
    return u.x * v.x + u.y * v.y;
}

double cross(const Point &u, const Point &v) {
    return u.x * v.y - u.y * v.x;
}

bool nearlyEqualPoints(const Point &p, const Point &q, double eps = EPS) {
    return dist(p, q) <= eps;
}

std::vector<Point> dedupeConsecutivePoints(const std::vector<Point> &pts) {
    std::vector<Point> out;
    out.reserve(pts.size());
    for (const Point &p : pts) {
        if (out.empty() || !nearlyEqualPoints(out.back(), p)) {
            out.push_back(p);
        }
    }
    return out;
}

std::vector<Segment> makeSegments(const std::vector<Point> &routePoints) {
    const std::vector<Point> pts = dedupeConsecutivePoints(routePoints);
    std::vector<Segment> segs;
    if (pts.size() < 2) return segs;
    segs.reserve(pts.size() - 1);
    for (size_t i = 1; i < pts.size(); ++i) {
        if (dist(pts[i - 1], pts[i]) > EPS) {
            segs.push_back({pts[i - 1], pts[i]});
        }
    }
    return segs;
}

bool isProperInteriorIntersection(const Segment &s1, const Segment &s2, Point &x) {
    double ix = 0, iy = 0;
    const int r = Avoid::segmentIntersectPoint(s1.a, s1.b, s2.a, s2.b, &ix, &iy);
    if (r != Avoid::DO_INTERSECT) return false;

    x = Point(ix, iy);

    const Point u(s1.b.x - s1.a.x, s1.b.y - s1.a.y);
    const Point v(s2.b.x - s2.a.x, s2.b.y - s2.a.y);
    if (std::fabs(cross(u, v)) <= EPS) return false;

    if (nearlyEqualPoints(x, s1.a) || nearlyEqualPoints(x, s1.b) ||
        nearlyEqualPoints(x, s2.a) || nearlyEqualPoints(x, s2.b)) {
        return false;
    }

    return true;
}

size_t countEdgeBends(const Edge_SP &e) {
    const std::vector<Point> pts = dedupeConsecutivePoints(e->getRoutePoints());
    if (pts.size() < 3) return 0;

    size_t bends = 0;
    for (size_t i = 1; i + 1 < pts.size(); ++i) {
        const Point d1(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
        const Point d2(pts[i + 1].x - pts[i].x, pts[i + 1].y - pts[i].y);
        if (dist(pts[i], pts[i - 1]) <= EPS || dist(pts[i + 1], pts[i]) <= EPS) continue;

        const double c = cross(d1, d2);
        const double d = dot(d1, d2);
        if (std::fabs(c) > EPS || d < 0) {
            ++bends;
        }
    }
    return bends;
}

double computeEdgeLength(const Edge_SP &e) {
    const std::vector<Point> pts = dedupeConsecutivePoints(e->getRoutePoints());
    if (pts.size() < 2) return 0.0;

    double len = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        len += dist(pts[i - 1], pts[i]);
    }
    return len;
}

bool shareEndpointNode(const Edge_SP &e1, const Edge_SP &e2) {
    const auto a = e1->getEndIds();
    const auto b = e2->getEndIds();
    return a.first == b.first || a.first == b.second || a.second == b.first || a.second == b.second;
}

std::string stripExtension(const std::string &path) {
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return path;
    if (slash != std::string::npos && dot < slash) return path;
    return path.substr(0, dot);
}

size_t countEdgeCrossings(const Graph &g) {
    std::vector<Edge_SP> edges;
    edges.reserve(g.getNumEdges());
    for (const auto &p : g.getEdgeLookup()) edges.push_back(p.second);

    size_t crossings = 0;

    for (size_t i = 0; i < edges.size(); ++i) {
        const Edge_SP &e1 = edges[i];
        const std::vector<Segment> s1 = makeSegments(e1->getRoutePoints());
        if (s1.empty()) continue;

        for (size_t j = i + 1; j < edges.size(); ++j) {
            const Edge_SP &e2 = edges[j];
            if (shareEndpointNode(e1, e2)) continue;

            const std::vector<Segment> s2 = makeSegments(e2->getRoutePoints());
            if (s2.empty()) continue;

            std::vector<Point> pairCrossings;
            for (const Segment &a : s1) {
                for (const Segment &b : s2) {
                    Point x;
                    if (!isProperInteriorIntersection(a, b, x)) continue;

                    bool seen = false;
                    for (const Point &y : pairCrossings) {
                        if (nearlyEqualPoints(x, y)) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) pairCrossings.push_back(x);
                }
            }
            crossings += pairCrossings.size();
        }
    }

    return crossings;
}

void printUsage() {
    std::cerr
        << "Usage: hola_metrics --input <input.tglf> "
        << "[--output-tglf <path>] [--output-svg <path>] "
        << "[--no-artifacts] "
        << "[--iel <value> | --iel-scalar <value>]"
        << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
    std::string inputPath;
    std::string outputTglfPath;
    std::string outputSvgPath;
    double iel = 0.0;
    bool hasIEL = false;
    double ielScalar = 1.0;
    bool noArtifacts = false;

    if (argc == 2 && std::string(argv[1]).rfind("--", 0) != 0) {
        inputPath = argv[1];
    } else {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--input" && i + 1 < argc) {
                inputPath = argv[++i];
            } else if (arg == "--output-tglf" && i + 1 < argc) {
                outputTglfPath = argv[++i];
            } else if (arg == "--output-svg" && i + 1 < argc) {
                outputSvgPath = argv[++i];
            } else if (arg == "--iel" && i + 1 < argc) {
                iel = std::stod(argv[++i]);
                hasIEL = true;
            } else if (arg == "--iel-scalar" && i + 1 < argc) {
                ielScalar = std::stod(argv[++i]);
            } else if (arg == "--no-artifacts") {
                noArtifacts = true;
            } else {
                printUsage();
                return 1;
            }
        }
    }

    if (inputPath.empty()) {
        printUsage();
        return 1;
    }

    if (hasIEL && !(iel > 0.0)) {
        std::cerr << "--iel must be > 0" << std::endl;
        return 1;
    }

    if (!(ielScalar > 0.0)) {
        std::cerr << "--iel-scalar must be > 0" << std::endl;
        return 1;
    }

    if (hasIEL && std::fabs(ielScalar - 1.0) > 1e-12) {
        std::cerr << "Use either --iel or --iel-scalar, not both" << std::endl;
        return 1;
    }

    Graph_SP graph;

    try {
        graph = buildGraphFromTglfFile(inputPath);
    } catch (const std::exception &e) {
        std::cerr << "Failed to load TGLF: " << e.what() << std::endl;
        return 2;
    }

    const double inferredIEL = graph->getIEL();
    double effectiveIEL = inferredIEL;

    if (hasIEL) {
        effectiveIEL = iel;
        graph->setIEL(effectiveIEL);
    } else if (std::fabs(ielScalar - 1.0) > 1e-12) {
        effectiveIEL = inferredIEL * ielScalar;
        graph->setIEL(effectiveIEL);
    }

    HolaOpts opts;
    const auto t0 = std::chrono::steady_clock::now();
    doHOLA(*graph, opts);
    const auto t1 = std::chrono::steady_clock::now();
    const double runtimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    size_t bends = 0;
    double totalEdgeLength = 0.0;
    for (const auto &p : graph->getEdgeLookup()) {
        const Edge_SP &e = p.second;
        bends += countEdgeBends(e);
        totalEdgeLength += computeEdgeLength(e);
    }
    const size_t edgeCount = graph->getNumEdges();
    const double avgEdgeLength = edgeCount > 0 ? totalEdgeLength / static_cast<double>(edgeCount) : 0.0;

    const size_t crossings = countEdgeCrossings(*graph);

    double area = 0.0;
    if (!graph->isEmpty()) {
        NodesById ignore;
        const bool includeBends = true;
        BoundingBox box = graph->getBoundingBox(ignore, includeBends);
        area = box.w() * box.h();
    }

    if (!noArtifacts) {
        if (outputTglfPath.empty() || outputSvgPath.empty()) {
            const std::string base = stripExtension(inputPath);
            if (outputTglfPath.empty()) outputTglfPath = base + ".hola.tglf";
            if (outputSvgPath.empty()) outputSvgPath = base + ".hola.svg";
        }

        writeStringToFile(graph->writeTglf(), outputTglfPath);
        writeStringToFile(graph->writeSvg(), outputSvgPath);
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "input=" << inputPath << std::endl;
    if (!noArtifacts) {
        std::cout << "output_tglf=" << outputTglfPath << std::endl;
        std::cout << "output_svg=" << outputSvgPath << std::endl;
    }
    std::cout << "nodes=" << graph->getNumNodes() << std::endl;
    std::cout << "edges=" << graph->getNumEdges() << std::endl;
    std::cout << "runtime_ms=" << runtimeMs << std::endl;
    std::cout << "edge_crossings=" << crossings << std::endl;
    std::cout << "edge_bends=" << bends << std::endl;
    std::cout << "area=" << area << std::endl;
    std::cout << "avg_edge_length=" << avgEdgeLength << std::endl;
    std::cout << "iel_scalar=" << ielScalar << std::endl;
    std::cout << "iel_inferred=" << inferredIEL << std::endl;
    std::cout << "iel_effective=" << effectiveIEL << std::endl;

    return 0;
}
